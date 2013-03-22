// http://ejohn.org/blog/ecmascript-5-strict-mode-json-and-more/
"use strict";

var fs = require('fs');

// parse command line for an ini file name
var ini_file='./aprs.ini';
if( process.argv[2] !== undefined) {
        ini_file=process.argv[2];
}

fs.exists(ini_file, function(exists) {
        if (!exists) {
                console.log('Can not open ' + ini_file + ' exiting');
                return;
        }
});

// Install node modules globally npm -g install <module_name>
var global_module_dir='/usr/local/lib/node_modules/';

// Optional. You can see this name displayed in 'ps' or 'top' command
process.title = 'track-server';

var mod_ctype = require(global_module_dir + 'ctype');

// *** Network socket server
var net = require('net');
var NETHOST = '127.0.0.1';

var UNIXPORT;
var HTMLPORT;
var NETPORT = 9123;  // default INET socket
var webSocketPort;

/**
 * Get config from ini file
 **/
var iniparser = require(global_module_dir + 'iniparser');
iniparser.parse(ini_file, function(err, data) {
        if (err) {
                console.log('Error: %s, failed to read %s', err.code, err.path);
                return;
        }
        /*
         * Dump all the ini file parameters
         */
//        var util = require('util');
//        var ds = util.inspect(data);
//        console.log('iniparse test, ui_net: ' + ds);
        webSocketPort = data.ui_net.websock_port;
        HTMLPORT = data.ui_net.html_port;
        if( data.ui_net.unix_socket != undefined ) {
                UNIXPORT = data.ui_net.unix_socket;
                NETPORT = UNIXPORT;
        }

//var broadcastPort = webSocketPort + 1;
var broadcastPort = 43256;
var TCP_DELIMITER = '\n';
var TCP_BUFFER_SIZE = Math.pow(2,16);


/**
 * Global variables
 */
// latest 100 messages
var history = [ ];
// list of currently connected clients (users)
var chatClients = [];

var netClients = [ ];
var wsClients = [ ];
var serverIPaddress='';


// websocket and http servers
var webSocketServer = require(global_module_dir + 'websocket').server;
var http = require('http');
var events = require("events");

var aprs_emitter = new events.EventEmitter();
var msg_emitter = new events.EventEmitter();

// Broadcast Server
var dgramServer = require('dgram');


/**
 * Helper function for escaping input strings
 */
function htmlEntities(str) {
	return String(str).replace(/&/g, '&amp;').replace(/</g, '&lt;')
			.replace(/>/g, '&gt;').replace(/"/g, '&quot;');
}

var getNetworkIP = (function () {
	var ignoreRE = /^(127\.0\.0\.1|::1|fe80(:1)?::1(%.*)?)$/i;

	var exec = require('child_process').exec;
	var cached;
	var command;
	var filterRE;

	switch (process.platform) {
		// TODO: implement for OSs without ifconfig command
	case 'darwin':
			command = 'ifconfig eth0';
			filterRE = /\binet\s+([^\s]+)/g;
			// filterRE = /\binet6\s+([^\s]+)/g; // IPv6
				      break;
	default:
		command = 'ifconfig eth0';
		filterRE = /\binet\b[^:]+:\s*([^\s]+)/g;
		// filterRE = /\binet6[^:]+:\s*([^\s]+)/g; // IPv6
			      break;
	}

	return function (callback, bypassCache) {
		// get cached value
		if (cached && !bypassCache) {
			callback(null, cached);
			return;
		}
		// system call
		exec(command, function (error, stdout, sterr) {
			var ips = [];
			// extract IPs
			var matches = stdout.match(filterRE);
			// JS has no lookbehind REs, so we need a trick
			for (var i = 0; i < matches.length; i++) {
				ips.push(matches[i].replace(filterRE, '$1'));
			}

			// filter BS
			for (var i = 0, l = ips.length; i < l; i++) {
				if (!ignoreRE.test(ips[i])) {
				    //if (!error) {
				    cached = ips[i];
			   	    //}
				    callback(error, ips[i]);
			 	    return;
				}
			}
			// nothing found
			callback(error, null);
		});
	};
})();
/*
   * Broadcast server
   */
getNetworkIP(function (error, ip) {
	console.log(ip);
	if (error) {
		console.log('error:', error);
	} else {
		serverIPaddress = ip;
	}
	console.log('Saving local ip address: ' + serverIPaddress);
	return ip;
}, false);


// Array with some colors, don't use the background color, currently orange
   var colors = [ 'red', 'green', 'blue', 'magenta', 'purple', 'plum', 'GreenYellow', 'DarkKhaki', 'Brown', 'SaddleBrown', 'SteelBlue' ];
// ... in random order
   colors.sort(function(a,b) { return Math.random() > 0.5; } );

if(UNIXPORT != undefined) {
        fs.unlink(UNIXPORT, function(err) {
                if (err) {
                        console.log('Will create socket:' + UNIXPORT );
                } else {
                        console.log('successfully deleted:' + UNIXPORT );
                };
        });
}

/**
 * HTTP server
 **/
var server = http.createServer(function(request, response) {
        // Not important for us. We're writing WebSocket server, not HTTP server
});
server.listen(webSocketPort, function() {
	console.log((new Date()) + " Server is listening on port " + webSocketPort);
});

/**
 * ===================== WebSocket server ========================
 */
var wsServer = new webSocketServer({
    // WebSocket server is tied to a HTTP server. WebSocket request is just
    // an enhanced HTTP request. For more info http://tools.ietf.org/html/rfc6455#page-6
	httpServer: server
});

wsClients.push(wsServer);

// This callback function is called every time someone
// tries to connect to the WebSocket server
wsServer.on('request', function(request) {

	console.log((new Date()) + ' Connection from origin ' + request.origin + '.');

	// accept connection - you should check 'request.origin' to make sure that
	// client is connecting from your website
	// (http://en.wikipedia.org/wiki/Same_origin_policy)
	var connection = request.accept(null, request.origin);
	// we need to know client index to remove them on 'close' event
	var index = chatClients.push(connection) - 1;
        var userName = false;
        var destName = false;
	var userColor = false;

	console.log((new Date()) + ' Connection accepted.');

	// send back chat history
	if (history.length > 0) {
		connection.sendUTF(JSON.stringify( { type: 'history', data: history} ));
	}

	//Test if anyone has logged in yet
	if(chatClients[0] !== undefined && chatClients[0].length !== undefined) {
		console.log("ws: Client length: " +  chatClients[0].length );
	} else {
		console.log("ws: NO Client");
	}

	// user sent some message
        connection.on('message', function(message) {
                if (message.type === 'utf8') { // accept only text

                        /* get data sent from users web page */
                        console.log('Parse message: ' + message.utf8Data);

                        var frontendmsg = JSON.parse(message.utf8Data);
                        console.log('Parse message 2: ' + frontendmsg.type);

                        if(frontendmsg.type === "setconfig") {
                                var json = JSON.stringify({ type:'setconfig', data: frontendmsg.data});
                                console.log('Sending SET config to radio: length =' + json.length + ' data: ' + frontendmsg.data);
                                msg_emitter.emit("aprs_cfg", json);

                        } else  if(frontendmsg.type === "getconfig") {
                                var json = JSON.stringify({ type:'getconfig', data: frontendmsg.data});
                                console.log('Sending GET config to radio: length =' + json.length + ' data: ' + frontendmsg.data);
                                msg_emitter.emit("aprs_cfg", json);

                        } else if(frontendmsg.type === "callsign") {

                                // remember user name
                                userName = htmlEntities(frontendmsg.data);
                                // get random color and send it back to the user
                                //userColor = colors.shift();
                                userColor='Maroon';
                                connection.sendUTF(JSON.stringify({ type:'color', data: userColor }));
                                console.log((new Date()) + ' User is known as: ' + userName
                                            + ' with ' + userColor + ' color.');

                        }  else if(frontendmsg.type === "sendto") {
                                // save destination name
                                destName = htmlEntities(frontendmsg.data);
                                connection.sendUTF(JSON.stringify({ type:'sendto', data: destName }));
                                console.log((new Date()) + ' Destination: ' + destName);

                        }
                        else if(frontendmsg.type === "message") {

                                // log and broadcast the message
                                console.log((new Date()) + ' Received Message from '
                                            + userName + ': ' + frontendmsg.data);

                                // we want to keep history of all sent messages
                                var obj = {
                                        time: (new Date()).getTime(),
                                              text: htmlEntities(frontendmsg.data),
                                              from: userName,
                                              sendto: destName,
                                              color: userColor
                                };

                                var histelements = history.length;
                                history.push(obj);
                                // keep a fixed length quueue
                                history = history.slice(-5);
                                console.log('history before: ' + histelements + ' after: ' + history.length);

                                // broadcast message to all connected clients
                                var json = JSON.stringify({ type:'message', data: obj });
                                for (var i=0; i < chatClients.length; i++) {
                                        chatClients[i].sendUTF(json);
                                }
r
                                connection.sendUTF(JSON.stringify({ type:'color', data: userColor }));

                                console.log('Sending message to radio: length =' + json.length);
                                /* Send message to radio to be transmitted via aprs */
                                delete obj.color;
                                var json = JSON.stringify({ type:'message', data: obj });
                                msg_emitter.emit("aprs_msg", json);
                                destName = false;

                        } else {
                                console.log('Unhandled message type from client: ' + frontendmsg.type);
                        }
                }
        });

	// user disconnected
	connection.on('close', function(connection) {
		if (userName !== false && userColor !== false) {
			console.log((new Date()) + " Peer "
				    + connection.remoteAddress + " disconnected.");
			// remove user from the list of connected clients
			chatClients.splice(index, 1);
			// push back user's color to be reused by another user
			colors.push(userColor);
		}
	});
});

/**
 * ===================== network Socket server ========================
 *
 * Create a server instance, and chain the listen function to it
 * The function passed to net.createServer() becomes the event handler for the 'connection' event
 * The sock object the callback function receives UNIQUE for each connection
 */
net.createServer(function(sock) {


	// We have a connection - a socket object is assigned to the connection automatically
	console.log('CONNECTED: ' + sock.remoteAddress +':'+ sock.remotePort);

	// To buffer tcp data see:
	// http://stackoverflow.com/questions/7034537/nodejs-what-is-the-proper-way-to-handling-tcp-socket-streams-which-delimiter
	var buf = new Buffer(TCP_BUFFER_SIZE);  //new buffer with size 2^16

//	sock.emit(news, {hello: world});
	netClients.push(sock);

	if(chatClients[0] !== undefined && chatClients[0].length !== undefined) {
		console.log("net0: Chat Client length: " +  chatClients[0].length );
	} else {
		console.log("net0: NO Chat Client");
	}
	var icount = 0;

	// 'data' event handler for this instance of socket
	sock.on('data', function(data) {
		var dataStr = data.toString('utf8');
/* #if 0
		console.log('DATA ' + sock.remoteAddress + ': ' + "data length: " + data.length + data.toString('utf8'));
#endif */

		var parser = new mod_ctype.Parser({ endian: 'little' });
		parser.typedef('msg_t', [
					   { type: { type: 'uint16_t' } },
					   { len: { type: 'uint16_t' } },
					   { namelen: { type: 'uint16_t' } },
					   { valuelen: { type: 'uint16_t' } }
		]);
/* #if 0
		parser.typedef('point_t', [
					   { type: { type: 'uint16_t' } },
					   { len: { type: 'uint16_t' } },
					   { namelen: { type: 'uint16_t' } },
					   { valuelen: { type: 'uint16_t' } },
					   { namevalstr: { type: 'char[2]' } }
		]);
		var out = parser.readData([ { point: { type: 'point_t' } } ], data, 0);
		console.log(out);

		var out1 = parser.readData([ { msg: { type: 'msg_t' } } ], data, out.point.len);
		console.log(out1);

		var parts = data.toString().split(TCP_DELIMITER);
#endif */
		var offset = 0;
		var i = 0;
		while(offset < data.length) {
			var out = parser.readData([ { point: { type: 'msg_t' } } ], data, offset);
// debug out		console.log(out);
/* #if 0
			console.log("check: " + i + " len: "+ out.point.len
				    + " name_len : " + out.point.namelen
				    + " value_len: " + out.point.valuelen
				    + data.slice(8+offset, 8+offset+out.point.namelen+out.point.valuelen) );
#endif */
/* #if debug out
			console.log('Part TEST ' + i + ' type: ' + out.point.type.toString(16) + " "
				    + 'len: ' + out.point.len.toString(16)
				    + " name[" + out.point.namelen + "]: " + data.slice(8+offset, 8+offset+out.point.namelen)
				    + " value[" + out.point.valuelen + "]: " + data.slice(8+offset+out.point.namelen, 8+offset+out.point.namelen+out.point.valuelen)
				   );
*/

			// Stringify the aprs_disp object
			var aprs_disp = {
				"name": "" + data.slice(8+offset, 8+offset+out.point.namelen),
				"value": "" + data.slice(8+offset+out.point.namelen, 8+offset+out.point.namelen+out.point.valuelen)
			}

			var aprsdisp_json = JSON.stringify(aprs_disp);
//			console.log("aprs_disp: " + aprsdisp_json );
			icount++;
			aprs_emitter.emit("aprs_disp", aprsdisp_json);

			// spew aprs packets to all connected clients
			//Test if anyone has logged in yet
			if(chatClients[0] !== undefined && chatClients[0].length !== undefined) {
			//var json = JSON.stringify({ type:'message', data: aprs_disp });
			//chatClients[0].sendUTF(json);
				console.log("net1: Client length: " +  chatClients[0].length );
			} else {
//				console.log("net1: NO Client");
			}


			offset += out.point.len;
			i++;

		}
/* #if 0
		offset = 0;

		for(var i = 0; i < parts.length-1; i++) {

			var msg = parser.readData([ { point: { type: 'point_t' } } ], data, offset);
			console.log("Loop: " + "offset: " + offset + ": " + msg.point.namevalstr);
			console.log("slice: "+ msg.point.namelen+ " - " + parts[i].slice(8) );
			offset+= msg.point.len;

			if(0) {
				console.log('Part TEST ' + i +' : ' + msgstruct.type.toString(16) + "" + '  ' + msgstruct.len.toString(16) + ' ' +
					    msgstruct.namelen.toString(16) + ' '+ msgstruct.valuelen.toString(16)
					    + ' '+ dataStr.substring(8,msgstruct.namelen+8)
					    + ' '+ dataStr.substring(msgstruct.namelen+8, msgstruct.len)
					   );
			} else {

			}
		}
#endif */
		/*
		console.log('TEST ' + ': ' + msgstruct.type.toString(16) + "" + '  ' + msgstruct.len.toString(16) + ' ' +
		msgstruct.namelen.toString(16) + ' '+ msgstruct.valuelen.toString(16)
		+ ' '+ dataStr.substring(8,msgstruct.namelen+8)
		+ ' '+ dataStr.substring(msgstruct.namelen+8, msgstruct.len)
		);
*/
		// Write the data back to the socket, the client will receive it as data from the server
		//		sock.write('You said "' + data + '"');

	});

	// Add a 'close' event handler to this instance of socket
	sock.on('close', function(data) {
		console.log('Unix socket connection closed at: ' + (new Date()) + 'socket address: ' + sock.remoteAddress +' '+ sock.remotePort);
	});

        aprs_emitter.on("aprs_disp", function(message) {

                if(chatClients[0] !== undefined && chatClients.length !== undefined) {
                        //			console.log("wsServer message: " + message);
                        var json1 = JSON.parse(message);
                        //			console.log(json1);
                        //			console.log("Name: " + json1.name + " Value: "+ json1.value);
                        var json = JSON.stringify({ type:'aprs', data: message });

                        for (var i=0; i < chatClients.length; i++) {
//                        console.log('aprs_emitter: sending to ' +  i + ' total: '+ chatClients.length);
                        chatClients[i].sendUTF(json);
                }
//                        var json = JSON.stringify({ type:'aprs', data: message });
//                        chatClients[0].sendUTF(json);

                }
        });

        msg_emitter.on("aprs_msg", function(message) {
                console.log('MSG: ' + message);
                sock.write(message);
        });

        msg_emitter.on("aprs_cfg", function(message) {
                console.log('CFG: ' + message);
                sock.write(message);
        });

}).listen(NETPORT, NETHOST);

console.log('Server listening on ' + NETHOST +':'+ NETPORT);

/* Every 5 seconds broadcast ip address */
/*
var clockID = setInterval(function() {
	var serverName = '#dantracker: ';
	var broadcastMessageSize = serverName.length + serverIPaddress.length +1;
	var broadcastThis = serverName + serverIPaddress;
	var broadcastMessage = new Buffer(broadcastThis);
//	broadcastMessage =  serverName + serverIPaddress;
	var baddr = '255.255.255.255';


//	console.log("DEBUG 1: sending datagram - " + broadcastMessage);
	console.log("DEBUG 1: sending datagram on address: " + baddr + " Port: " + broadcastPort);

	var client = dgramServer.createSocket("udp4");
	client.bind(broadcastPort);
	client.setBroadcast(true);

	var address = client.address();
//	console.log("DEBUG 2: server listening " + address.address + ":" + address.port);

	client.send(broadcastMessage, 0, broadcastMessage.length, broadcastPort, baddr, function(err, bytes) {
		client.close();
	});

}, 20000);

*/

/**
 * ===================== HTML server ========================
 */

var connect = require(global_module_dir + 'connect');
connect.createServer(
                     connect.static(__dirname),
                     function(req, res){
        res.setHeader('Content-Type', 'text/html');
        res.end('You need a path, try /tracker.html\n');
}
).listen(HTMLPORT);

});

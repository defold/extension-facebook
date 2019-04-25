#!/usr/bin/env python

import string, cgi, time, os, ssl, sys
from BaseHTTPServer import BaseHTTPRequestHandler, HTTPServer

class MyRequestHandler(BaseHTTPRequestHandler):

    def do_GET(self):
        try:
            if "hub.verify_token" in self.path:
                # fb test hooks
                self.send_response(200)
                self.send_header("Content-type", "text/html")
                self.end_headers()
                querys = dict(cgi.parse_qsl(self.path))
                verify_token = "tokentest"
                if querys["hub.verify_token"] == verify_token:
                    self.wfile.write(str(querys["hub.challenge"]))
                return

            if self.path.endswith("/postwebhook"):
                self.send_response(200)
                self.send_header("Content-type", "text/html")
                self.end_headers()
                self.wfile.write("ok")
                return

            # your logic for this URL here, such as executing a bash script
            file_path = os.path.abspath(self.path.lstrip("/"))
            if os.path.isfile(file_path):
                self.send_response(200)
                self.send_header("Content-type", "text/html")
                self.end_headers()
                f=open(file_path)
                self.wfile.write(f.read())
                f.close()
                return

            else:
                self.send_error(404, "File Not Found %s" % self.path)

        except:
            self.send_error(404, "File Not Found %s" % self.path)


    def do_POST(self):
        try:
            if self.headers["Content-type"] == "application/json":
                self.send_response(200)
                length = int(self.headers.getheader('content-length'))
                print 'content-data:', str(self.rfile.read(length))
                return
        except:
            pass
        self.do_GET()


def main(port, cert=None, key=None):
    try:
        server = HTTPServer(("", port), MyRequestHandler)
        is_https = False
        if cert is not None and key is not None:
            if os.path.exists(cert) and os.path.exists(key):
                server.socket = ssl.wrap_socket(server.socket, certfile=cert, keyfile=key, server_side = True)
                is_https = True
        else:
            print "Couldn't find cert.pem and/or key.pem, falling back to http://"
        print "Server started at " + ("https" if is_https else "http") + "://localhost:%d" % port
        print "Press ^C to quit."
        server.serve_forever()
    except KeyboardInterrupt:
        print " received, shutting down server"

    server.socket.close()

if __name__ == "__main__":
    port = 8000
    if len(sys.argv) >= 2:
        port = int(sys.argv[1])

    cert = "cert.pem"
    if len(sys.argv) >= 3:
        cert = sys.argv[2]

    key = "key.pem"
    if len(sys.argv) >= 4:
        key = sys.argv[3]

    main(port, cert, key)

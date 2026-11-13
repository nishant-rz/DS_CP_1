import http.server
import socketserver
import urllib.parse
import subprocess
import sys

PORT = 8000

class MissionControlHandler(http.server.SimpleHTTPRequestHandler):
    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        if parsed.path == "/api/scan":
            params = urllib.parse.parse_qs(parsed.query)
            mode = params.get("mode", ["replay"])[0]
            print(f"\n[WEB TRIGGER] Running Satellite Pass in '{mode}' mode...")
            subprocess.run([sys.executable, "fetch_satellite.py", mode])
            self.send_response(200)
            self.send_header("Content-type", "application/json")
            self.end_headers()
            self.wfile.write(b'{"status": "ok"}')
            return
        return super().do_GET()

if __name__ == "__main__":
    print(f"============================================================")
    print(f" RESQNET MISSION CONTROL LIVE AT: http://localhost:{PORT}")
    print(f"============================================================")
    with socketserver.TCPServer(("", PORT), MissionControlHandler) as httpd:
        httpd.serve_forever()
#!/usr/bin/env python3

#NOTE - THIS STILL HAS TESTFILE CREATION CODE IN IT - Vansh

import os
import sys
import json
import time
import argparse
import mimetypes
import shutil
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse, parse_qs

class CacheAPIHandler(BaseHTTPRequestHandler):

    def _send_json_response(self, code, data):
        self.send_response(code)
        self.send_header('Content-Type', 'application/json')
        self.end_headers()
        self.wfile.write(json.dumps(data).encode('utf-8'))

    def _send_error_response(self, code, message):
        self.send_response(code)
        self.send_header('Content-Type', 'application/json')
        self.end_headers()
        self.wfile.write(json.dumps({'error': message}).encode('utf-8'))

    def _get_file_info(self, path):
        full_path = os.path.join(self.server.root_dir, path.lstrip('/'))
        
        if not os.path.exists(full_path):
            return None
            
        stat_info = os.stat(full_path)
        is_dir = os.path.isdir(full_path)
        
        return {
            'name': os.path.basename(path) or '/',
            'size': 0 if is_dir else stat_info.st_size,
            'mtime': int(stat_info.st_mtime),
            'is_directory': is_dir
        }

    def do_GET(self):
        url_parts = urlparse(self.path)
        path = url_parts.path
        
        if path.startswith('/api/info/'):
            self._handle_info_request(path[9:])
        elif path.startswith('/api/list/'):
            self._handle_list_request(path[9:])
        elif path.startswith('/api/data/'):
            self._handle_data_request(path[9:])
        else:
            self._send_error_response(404, "Not Found")

    def do_PUT(self):
        url_parts = urlparse(self.path)
        path = url_parts.path
        
        if path.startswith('/api/data/'):
            self._handle_data_upload(path[9:], 'PUT')
        else:
            self._send_error_response(404, "Not Found")

    def do_PATCH(self):
        url_parts = urlparse(self.path)
        path = url_parts.path
        
        if path.startswith('/api/data/'):
            self._handle_data_upload(path[9:], 'PATCH')
        else:
            self._send_error_response(404, "Not Found")

    def do_POST(self):
        url_parts = urlparse(self.path)
        path = url_parts.path
        
        if path.startswith('/api/create/'):
            self._handle_create_request(path[11:])
        elif path == '/api/rename':
            self._handle_rename_request()
        else:
            self._send_error_response(404, "Not Found")

    def do_DELETE(self):
        url_parts = urlparse(self.path)
        path = url_parts.path
        
        if path.startswith('/api/delete/'):
            self._handle_delete_request(path[11:])
        else:
            self._send_error_response(404, "Not Found")

    def _handle_info_request(self, path):
        info = self._get_file_info(path)
        
        if info:
            self._send_json_response(200, info)
        else:
            self._send_error_response(404, f"File not found: {path}")

    def _handle_list_request(self, path):
        full_path = os.path.join(self.server.root_dir, path.lstrip('/'))
        
        if not os.path.isdir(full_path):
            self._send_error_response(404, f"Directory not found: {path}")
            return
            
        try:
            entries = []
            for name in os.listdir(full_path):
                entry_path = os.path.join(path, name)
                info = self._get_file_info(entry_path)
                if info:
                    entries.append(info)
                    
            self._send_json_response(200, entries)
        except Exception as e:
            self._send_error_response(500, str(e))

    def _handle_data_request(self, path):
        full_path = os.path.join(self.server.root_dir, path.lstrip('/'))
        
        if not os.path.isfile(full_path):
            self._send_error_response(404, f"File not found: {path}")
            return
            
        try:
            range_header = self.headers.get('Range')
            
            if range_header:
                range_match = range_header.strip().lower()
                if range_match.startswith('bytes='):
                    ranges = range_match[6:].split('-')
                    start = int(ranges[0]) if ranges[0] else 0
                    end = int(ranges[1]) if len(ranges) > 1 and ranges[1] else None
                    
                    file_size = os.path.getsize(full_path)
                    
                    if end is None:
                        end = file_size - 1
                    
                    end = min(end, file_size - 1)

                    content_length = end - start + 1
                    
                    self.send_response(206)
                    self.send_header('Content-Range', f'bytes {start}-{end}/{file_size}')
                    self.send_header('Content-Length', str(content_length))
                    
                    content_type, _ = mimetypes.guess_type(full_path)
                    if content_type:
                        self.send_header('Content-Type', content_type)
                    self.end_headers()
                    
                    with open(full_path, 'rb') as f:
                        f.seek(start)
                        self.wfile.write(f.read(content_length))
                    
                    return
            
            with open(full_path, 'rb') as f:
                file_data = f.read()
                
            self.send_response(200)
            content_type, _ = mimetypes.guess_type(full_path)
            if content_type:
                self.send_header('Content-Type', content_type)
            self.send_header('Content-Length', str(len(file_data)))
            self.end_headers()
            self.wfile.write(file_data)
            
        except Exception as e:
            self._send_error_response(500, str(e))

    def _handle_data_upload(self, path, method):
        full_path = os.path.join(self.server.root_dir, path.lstrip('/'))
        directory = os.path.dirname(full_path)
        
        try:
            if not os.path.exists(directory):
                os.makedirs(directory, exist_ok=True)
                
            content_length = int(self.headers.get('Content-Length', 0))
            
            data = self.rfile.read(content_length)

            if method == 'PATCH' and 'Content-Range' in self.headers:
                range_header = self.headers['Content-Range']
                parts = range_header.split(' ')[1].split('-')
                start = int(parts[0])
                
                with open(full_path, 'r+b' if os.path.exists(full_path) else 'wb') as f:
                    f.seek(start)
                    f.write(data)
                    
                self.send_response(204)
                self.end_headers()
            else:
                with open(full_path, 'wb') as f:
                    f.write(data)
                    
                self.send_response(201 if method == 'PUT' else 200)
                self.end_headers()
                
        except Exception as e:
            self._send_error_response(500, str(e))

    def _handle_create_request(self, path):
        """Handle /api/create POST requests"""
        full_path = os.path.join(self.server.root_dir, path.lstrip('/'))
        
        query = parse_qs(urlparse(self.path).query)
        is_directory = 'directory' in query and query['directory'][0].lower() == 'true'
        
        try:
            if is_directory:
                os.makedirs(full_path, exist_ok=True)
            else:
                directory = os.path.dirname(full_path)
                if not os.path.exists(directory):
                    os.makedirs(directory, exist_ok=True)
                
                if not os.path.exists(full_path):
                    with open(full_path, 'wb') as f:
                        pass
            
            self.send_response(201)
            self.end_headers()
        except Exception as e:
            self._send_error_response(500, str(e))

    def _handle_delete_request(self, path):
        """Handle /api/delete DELETE requests"""
        full_path = os.path.join(self.server.root_dir, path.lstrip('/'))
        
        if not os.path.exists(full_path):
            self._send_error_response(404, f"Path not found: {path}")
            return
            
        query = parse_qs(urlparse(self.path).query)
        is_directory = 'directory' in query and query['directory'][0].lower() == 'true'
        
        try:
            if is_directory and os.path.isdir(full_path):
                shutil.rmtree(full_path)
            elif not is_directory and os.path.isfile(full_path):
                os.remove(full_path)
            else:
                self._send_error_response(400, "Invalid request: path type mismatch")
                return
                
            self.send_response(204)
            self.end_headers()
        except Exception as e:
            self._send_error_response(500, str(e))

    def _handle_rename_request(self):
        content_length = int(self.headers.get('Content-Length', 0))
        body = self.rfile.read(content_length).decode('utf-8')
        
        try:
            data = json.loads(body)
            old_path = data.get('old_path', '')
            new_path = data.get('new_path', '')
            
            if not old_path or not new_path:
                self._send_error_response(400, "Missing old_path or new_path")
                return
                
            old_full_path = os.path.join(self.server.root_dir, old_path.lstrip('/'))
            new_full_path = os.path.join(self.server.root_dir, new_path.lstrip('/'))
            
            if not os.path.exists(old_full_path):
                self._send_error_response(404, f"Source path not found: {old_path}")
                return
                
            new_parent_dir = os.path.dirname(new_full_path)
            if not os.path.exists(new_parent_dir):
                os.makedirs(new_parent_dir, exist_ok=True)
                
            os.rename(old_full_path, new_full_path)
            
            self.send_response(204)
            self.end_headers()
            
        except json.JSONDecodeError:
            self._send_error_response(400, "Invalid JSON body")
        except Exception as e:
            self._send_error_response(500, str(e))
    
    def log_message(self, format, *args):
        sys.stderr.write("%s - [%s] %s\n" %
                         (self.client_address[0],
                          self.log_date_time_string(),
                          format % args))

class CacheServer(HTTPServer):
    def __init__(self, server_address, handler_class, root_dir):
        super().__init__(server_address, handler_class)
        self.root_dir = os.path.abspath(root_dir)

def create_test_files(directory, sizes_kb=None):
    if sizes_kb is None:
        sizes_kb = [1, 10, 100, 1000]
        
    for size_kb in sizes_kb:
        filename = os.path.join(directory, f"test_{size_kb}kb.txt")
        with open(filename, 'w') as f:
            content = f"This is test file of size {size_kb}KB. "
            repeat_count = max(1, int(size_kb * 1024 / len(content)))
            f.write(content * repeat_count)
        
        print(f"Created {filename} ({size_kb} KB)")
    
    test_dir = os.path.join(directory, "test_dir")
    os.makedirs(test_dir, exist_ok=True)
    
    for i in range(5):
        with open(os.path.join(test_dir, f"file_{i}.txt"), 'w') as f:
            f.write(f"This is file {i} in test_dir\n")
    
    print(f"Created directory {test_dir} with 5 files")

def main():
    parser = argparse.ArgumentParser(description='Local HTTP server for FUSE-based remote file caching')
    parser.add_argument('--port', type=int, default=8080, help='Server port')
    parser.add_argument('--directory', type=str, default='./test_data', help='Directory to serve')
    parser.add_argument('--create-test-files', action='store_true', help='Create test files in the directory')
    args = parser.parse_args()
    
    os.makedirs(args.directory, exist_ok=True)
    
    if args.create_test_files:
        create_test_files(args.directory)
    
    server = CacheServer(('', args.port), CacheAPIHandler, args.directory)
    server_address = f"http://localhost:{args.port}"
    
    print(f"Starting server at {server_address}")
    print(f"Serving from directory: {os.path.abspath(args.directory)}")
    print("API endpoints:")
    print(f"  GET    {server_address}/api/info/[path]     - Get file info")
    print(f"  GET    {server_address}/api/list/[path]     - List directory contents")
    print(f"  GET    {server_address}/api/data/[path]     - Download file")
    print(f"  PUT    {server_address}/api/data/[path]     - Upload file")
    print(f"  PATCH  {server_address}/api/data/[path]     - Update file")
    print(f"  POST   {server_address}/api/create/[path]   - Create file or directory")
    print(f"  DELETE {server_address}/api/delete/[path]   - Delete file or directory")
    print(f"  POST   {server_address}/api/rename          - Rename/move file or directory")
    print("\nPress Ctrl+C to stop the server")
    
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nServer stopped.")

if __name__ == "__main__":
    main()
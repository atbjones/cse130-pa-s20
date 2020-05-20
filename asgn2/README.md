run make to create executable httpserver.

execute ./httpserver <port>
options:
	-l <log_file>: The server will create a file and output a log of all requests it received
	-N <number>: Tell the server to use a specific number of threads. Defaults to 4

test with curl commands in test/client/script

s_cl:
	g++ chat_server.cpp -o chat_server -l boost_system -l pthread
s_run:
	./chat_server 2019
s_cr:
	g++ chat_server.cpp -o chat_server -l boost_system -l pthread
	./chat_server 2019
	
c_cl:
	g++ chat_client.cpp -o chat_client -l boost_system -l pthread
c_run:
	./chat_client "127.0.0.1" 2019

all: ts tc

ts:
	g++ -o echo-server ts.cpp -pthread

tc:
	g++ -o echo-client tc.cpp

clean:
	rm -f echo-client
	rm -f echo-server

.PHONY: clean

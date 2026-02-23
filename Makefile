debug:
	gcc -g -o app/http-server app/main.c app/http.c

clean:
	rm -f app/http-server

create-test:
	touch $(dir $(file))$(basename $(notdir $(file)))-test.c
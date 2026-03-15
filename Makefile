debug:
	gcc -g -o app/http-server app/main.c app/http.c app/file.c

clean:
	rm -f app/http-server

create-test:
	touch $(dir $(file))$(basename $(notdir $(file)))-test.c
debug:
	gcc -g -o app/http-server app/main.c app/http.c

create-test:
	touch $(dir $(file))$(basename $(notdir $(file)))-test.c
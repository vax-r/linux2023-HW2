FROM ubuntu:latest

RUN apt-get update -y \
&& apt-get install vim -y \
&& apt-get install nginx -y \
&& apt-get install make -y \
&& apt-get install gcc -y

ADD . /code

CMD ["nginx", "-g", "daemon off;"]
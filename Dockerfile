FROM ubuntu:latest

RUN apt-get update -y \
&& apt-get install vim -y \
&& apt-get install nginx -y

ADD . /code

CMD ["nginx", "-g", "daemon off;"]
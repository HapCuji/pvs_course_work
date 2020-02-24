#!/bin/bash
((
echo open 127.0.0.1 1997
sleep 1
echo "EHLO my-test.ru"
sleep 1
echo "VRFY <@my-test.ru>"
sleep 1
echo "MAIL from: <a@yandex.ru>"
sleep 1
echo "RCPT to: <b@mail.ru>"
sleep 1
echo "DATA"
sleep 1
echo "Hello, cruel world!"
sleep 1
echo "Im sleep.."
sleep 1
echo "."
sleep 1
echo "QUIT"
sleep 1
) | telnet)

#in thread or main was error in free() after error cmd sequence
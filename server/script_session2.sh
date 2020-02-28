#!/bin/bash
((
echo open 127.0.0.1 1997
sleep 1
echo "helo olol-test.ru"
sleep 1
echo "MAIL from: <kob@mail.ru>"
sleep 1
echo "rcpt to: <olo@olo1.ru> <mum@mum.ru> <tree@gogo.de><dad@get.com> <pornohub@success.t> <tree@gogo.de>"
sleep 1
echo "DATA"
sleep 1
echo "WHO IS IT!?"
sleep 1
echo "go away! don't read here!"
sleep 1 # without or need use just first "./r/n"
echo "."
sleep 5
echo "QUIT"
sleep 1
) | telnet)

#in thread or main was error in free() after error cmd sequence
#ifndef __SMTP_DEF_H__
#define __SMTP_DEF_H__

// smtp keys code
// #define KEY_FAILED -1
// #define KEY_HELO 0
// #define KEY_EHLO 1
// #define KEY_MAIL 2
// #define KEY_RCPT 3
// #define KEY_DATA 4
// #define KEY_NOOP 5
// #define KEY_RSET 6
// #define KEY_QUIT 7
// #define KEY_VRFY 8

//----------------------
// smtp keys string
//----------------------
#define STR_HELO                "HELO "
#define STR_EHLO                "EHLO "
#define STR_MAIL                "MAIL FROM:"
#define STR_RCPT                "RCPT TO:"
#define STR_DATA                "DATA"
#define STR_NOOP                "NOOP "
#define STR_RSET                "RSET "
#define STR_QUIT                "QUIT "
#define STR_VRFY                "VRFY "

// replies smtp server
// change <имя_домена> - "kob.ru" domain name server mta
// 2xx
#define REPLY_START             "220 SMTP bmstu kob.ru Transfer Service Ready\r\n"  
#define REPLY_EHLO              "250-VRFY\r\n250-NOOP\r\n250-RSET\r\n"
#define REPLY_QUIT              "221 close connection\r\n"
#define REPLY_HELO              "250 kob.ru\r\n"    // for mail from: rcpt to:/
#define REPLY_OK                "250 OK\r\n"    // for mail from: rcpt to:/
#define REPLY_MAIL_OK           REPLY_OK
#define REPLY_RCPT_OK           REPLY_OK
#define REPLY_ADMIN             "252 Administrative prohibition\r\n"
// 3xx          
#define REPLY_DATA              "354 Enter message, ending with \".\" on a line by itself\r\n"
// 4xx          
#define REPLY_TERMINATE         "421 closing server\r\n"
#define REPLY_UN_MAIL           "450 mailbox unavailable\r\n"
#define REPLY_MUCH_REC          "451 too much recepients\r\n"
#define REPLY_MEM               "452 we need more gold\r\n"
// 5xx          
#define REPLY_UNREC             "500 Unrecognized command\r\n"
#define REPLY_BAD_ARGS          "501 invalid argument(s)\r\n"
#define REPLY_UNREALIZED        "502 This command has not been implemented yet\r\n"
#define REPLY_SEQ               "503 Wrong command sequence\r\n"
#define REPLY_TOO_MANY_ERROR    "503 Too many wrong command\r\n"

#endif // !__SMTP_DEF_H__

/*
Код	
Значение

211	Ответ о состоянии системы или помощь
214	Сообщение-подсказка (помощь)
220	<имя_домена> служба готова к работе
221	<имя_домена> служба закрывает канал связи
250	Запрошенное действие почтовой транзакции успешно завершилось
251	Данный адресат не является местным; сообщение будет передано по маршруту <forward-path>

354	Начинай передачу сообщения. Сообщение заканчивается комбинацией CRLF-точка-CRLF

421	<имя_домена> служба недоступна; соединение закрывается
450	Запрошенная команда почтовой транзакции не выполнена, так как почтовый ящик недоступен
451	Запрошенная команда не выполнена; произошла локальная ошибка при обработке сообщения
452	Запрошенная команда не выполнена; системе не хватило ресурсов

500	Синтаксическая ошибка в тексте команды; команда не опознана
501	Синтаксическая ошибка в аргументах или параметрах команды
502	Данная команда не реализована
503	Неверная последовательность команд
504	У данной команды не может быть аргументов
550	Запрошенная команда не выполнена, так как почтовый ящик недоступен
551	Данный адресат не является местным; попробуйте передать сообщение по маршруту <forward-path>
552	Запрошенная команда почтовой транзакции прервана; дисковое пространство, доступное системе, переполнилось
553	Запрошенная команда не выполнена; указано недопустимое имя почтового ящика
554	Транзакция не выполнена
*/
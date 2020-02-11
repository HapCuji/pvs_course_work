#include <mta_server.h>

/*
 если клиен будет использовать рабочие процессы, то проблемы
 - использовать 1 определенный сокет для 1 определенной МХ записи (он может контролировать и больше)
 - использовать очереди можно раздавая задания с одной МХ записью 
 (но тогда должн быть флаг что письман на этот МХ кончлись и сокет можно закрыть (и после этого его сможет открыть другой проццесс если надо (должны уложться в одну ссесию)))

+ для потоков должно быть проще так как набор сокетов будет один
- можно каждому сокету в нагрузку даватьна отправление определенные данные от каждого из процессов
    - в этом случае очередь для чтения будет организована папками (сколлько воркеров столько и папок?) 
    1. кладет внутрь по очереди (можно и для процессов) 2. или читаем с помощью мьютексов 3. создаем папки по конкресному имени домена

here must be main proc with threads
    every threads read letters in /maildir/..
    (mb nessesary to catch mutex() for reading maildir or localdir)
        => must be pull first letter
        and read MX inside
            get mutex with this (if mutes(MX) busy => break)        // here mutex will be just general data for all threads
                if it MX is exist (in DNS answer)
                    => while not all letters readed 
                        => try send all letters 
                            with the same MX in one session



*/
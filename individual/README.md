# HTTP-прокси

В зависимостях буст. Хедеры для `Boost.Asio` и хедеры и либы для `Boost.IOStreams`.

Запускается так:

```
$ ./http_proxy localhost 8008
```

Протестировать можно так:

```
$ http_proxy="http://localhost:8008/" curl vasalf.net
```

Можно ещё в системных настройках настроить http-proxy и любоваться на открывающиеся странички в браузере, всплывающие в логе приложения.

В силу ограниченности современного использования http (без https!), я тестировал всего на паре сайтов (`vasalf.net`, `minisat.se`, `http://www.google.ru`, причём последний работает только через курл, так как браузер в курсе, кто такой гугл и что гугл может в https).

Кстати, для гугла я даже поддержал chunked encoding.

## Кеширование

Включается если в ответе сервера в `Cache-Control` написано что-то разумное, разрешающее такие махинации. Потестить можно так:

```
$ http_proxy="http://localhost:8008/" curl minisat.se
$ http_proxy="http://localhost:8008/" curl minisat.se
```

Лог сервера расскажет, что во второй раз он ответил закешированной копией, что и будет происходить в ближайшие десять минут. Можно ещё на практике заметить, что курл завершается заметно быстрее в этот период времени.

## Сжатие

Включается, если в запросе передать `Accept-Encoding: gzip`. Ну или что-то, содержащее `gzip`.

Можно было бы пофильтровать по `Content-Type` и сжимать только картинки. Я решил просто сжимать всё.

Потестировать можно так:

```
$ http_proxy="http://localhost:8080/" curl --compressed vasalf.net
```

В учебных целях я на всякий случай удаляю из запроса к самому серверу `Accept-Encoding`.

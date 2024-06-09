# Plain Text NETTT Protocol

## Symbols

`<SEPARATOR>` - single character - ":"

## General message format & representation

Messages are sent in format:

"[MESSAGE TYPE]"\<SEPARATOR\>"[DATA]"

With max lenght message lenght of 64 with null-terminator

## Message type

| Message Type |         Data Contents          |                                              Function                                               |
|--------------|--------------------------------|-----------------------------------------------------------------------------------------------------|
| LEAVE        | NONE                           | Client notifies server that he's about to close TCP connection and wants to leave the game.         |
| OK           | NONE or Initialization message | Sender acknowledges previous message from receiver. Is also used as Initialization message.         |
| ERR          | Error message                  | Indicates error - either internal or as a reaction to receiver previous message.                    |
| MOVE         | Field number [1-9]             | Either tells server where client wants to move or tells the client where did the enemy make a move. |
| STATE        | STATE string representation    | State of the game according to server.                                                              |
| END          | NONE                           | Server notifies of the end of the game.                                                             |

## Game State

One of:

- PLAYING,
- X_WON,
- O_WON,
- DRAW

Self explenatory

## Example game

c1, c2 - clients

s - server

c1 -> s connect()

c2 -> s connect()

c1 <- s OK:X

c2 <- s OK:O

c1 -> s MOVE:1

c2 <- s MOVE:1

c1 <- s STATE:PLAYING

c2 <- s STATE:PLAYING

c2 -> s MOVE:5

c1 <- s MOVE:5

c1 <- s STATE:PLAYING

c2 <- s STATE:PLAYING

... untill the winning move is made or it's draw:

c2 -> s MOVE:9

c1 <- s MOVE:9

c1 <- s STATE:O_WON

c2 <- s STATE:O_WON

c1 <- s END:

c2 <- s END:

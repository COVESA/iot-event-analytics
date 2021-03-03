# MQTT publisher

## How to use it

- ```node cli.js pub -c "mqtt://localhost:1883" -t "ingestion/events" -f msg.txt --times 5 --delayMs 1500```
  Make sure whenMS is a point of time in the future. Otherwise the event is treated as outdated

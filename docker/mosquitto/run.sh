#!/bin/ash

# Create the mosquitto configuration file
mustach /mosquitto/config/config.json /mosquitto/templates/mosquitto.mustache > /mosquitto/config/mosquitto.conf

# Print the contents
cat /mosquitto/config/mosquitto.conf

# Execute mosquitto
exec /usr/sbin/mosquitto -c /mosquitto/config/mosquitto.conf
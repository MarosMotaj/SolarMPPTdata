#!/usr/bin/env python
import time
from paho.mqtt import client as mqtt_client


class Subscriber:

    def __init__(self, ip_address, port_number, topic_volts, topic_amper,
                 topic_power, client_id, username, password, the_queue):
        self.the_queue = the_queue
        self.ip_address = ip_address
        self.port_number = port_number
        self.topic_volts = topic_volts
        self.topic_amper = topic_amper
        self.topic_power = topic_power
        self.client_id = client_id
        self.username = username
        self.password = password

    def connect_mqtt(self):
        def on_connect(client, userdata, flags, rc):
            if rc == 0:
                print("Connected to MQTT Broker!")
            else:
                print("Failed to connect, return code %d\n", rc)

        client = mqtt_client.Client(self.client_id)
        client.username_pw_set(self.username, self.password)
        client.on_connect = on_connect
        client.connect(self.ip_address, self.port_number)
        return client

    def subscribe(self, client: mqtt_client):

        def on_message(client, userdata, msg):
            print(f"MPPT data: `{msg.payload.decode()}` from `{msg.topic}`")

            if f"{msg.payload.decode()}" == "%ld":
                self.the_queue.put("0")
            else:
                self.the_queue.put(f"{msg.payload.decode()}")

        client.subscribe(self.topic_volts)
        client.subscribe(self.topic_amper)
        client.subscribe(self.topic_power)
        client.on_message = on_message

    def run(self):
        client = self.connect_mqtt()
        self.subscribe(client)
        client.loop_forever()






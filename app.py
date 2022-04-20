#!/usr/bin/env python
import kivy
from kivy.app import App
from kivy.uix.boxlayout import BoxLayout
from kivy.clock import Clock
from kivy.core.window import Window
from subscriber import Subscriber
from config import const
from threading import Thread
import threading
from queue import Queue

# Verzia mobilu treba sa pohrat ak nejde..
kivy.require('1.9.0')


class MyRoot(BoxLayout):

    def __init__(self):
        super(MyRoot, self).__init__()
        Window.size = (1080, 2520)
        self.subscriber_queue = Queue()
        self.subscriber_thread = None

        self.subscriber = Subscriber(const.IP_ADDRESS,
                                     const.PORT,
                                     const.TOPIC_VOLTS,
                                     const.TOPIC_AMPER,
                                     const.TOPIC_POWER,
                                     const.CLIENT_ID,
                                     const.USERNAME,
                                     const.PASSWORD,
                                     self.subscriber_queue)
        self.start_subscriber()

    def start_subscriber(self):
        self.subscriber_thread = Thread(target=self.subscriber.run)
        self.subscriber_thread.daemon = True
        Clock.schedule_once(self.refresh_kiwi_data, 1)
        self.subscriber_thread.start()

    def refresh_kiwi_data(self, dt):

        if not self.subscriber_thread.is_alive() and not self.subscriber_queue.empty():
            return

        # obnov GUI novymi datami z queue
        while not self.subscriber_queue.empty():
            mqtt_volt_data = self.subscriber_queue.get()
            mqtt_amper_data = self.subscriber_queue.get()
            mqtt_power_data = self.subscriber_queue.get()

            self.ids.power_progress.text = f'{mqtt_power_data} kW'
            self.ids.power_progress.progress = mqtt_power_data
            self.ids.voltage_progress.text = f'{mqtt_volt_data} V'
            self.ids.voltage_progress.progress = mqtt_volt_data
            self.ids.ampere_progress.text = f'{mqtt_amper_data} A'
            self.ids.ampere_progress.progress = mqtt_amper_data

        Clock.schedule_once(self.refresh_kiwi_data, 1)

    # Testovacia metoda
    def print_alive_threads(self):
        for thread in threading.enumerate():
            print(thread.getName())


class MyApp(App):
    """Vstavana funkcia kivi frameworku"""

    def build(self):
        self.title = "Solar regulator data program"
        return MyRoot()

import socket
import sys
import threading
import select
import struct
import datetime
from ctypes import *
from random import randint
import time
import os

#enconding: utf-8
x=0
j=2
k=3

def server_configuration():
    fitxer_configuracio=open("server.cfg", "r")

    server_config_dades=[]
    for line in fitxer_configuracio:
        server_config_dades.append(line.split()[1])

    fitxer_configuracio.close()

    server_config = {
        "nom": server_config_dades[0],
        "mac": server_config_dades[1],
        "udp_port": server_config_dades[2],
        "tcp_port": server_config_dades[3]
    }

    return server_config
    
def get_clients_autoritzats():

    fitxer_clients = open("equips.dat", "r")
    llista_clients=[]
    for line in fitxer_clients:
        client=Client(line.split()[0],line.split()[1],"DISCONNECTED","000000","",False,0)
        llista_clients.append(client)
        
    return llista_clients

def data_treatment(data_struct):
    data=POINT.from_buffer_copy(data_struct)
    return data

def debuger(msg):
    now = datetime.datetime.now()
    time = now.strftime("%H:%M:%S")
    print(time+" DEBUG => "+msg)

def reply(data, addr, clients, server_config, sock):

    client=check_client(data,clients)
    if data.tipus_paquet==0x00:
        debuger("PDU Rebuda: REGISTER_REQ")
        check_register(data, addr, client, server_config, sock)
    if data.tipus_paquet==0x10:
        debuger("PDU Rebuda: ALIVE_INF")
        check_alive(data, addr, client, server_config, sock)

def check_client(data,clients):
    client=Client("0000000","0000000000000","DISCONNECTED","000000","",False,0)
    for c in clients:
        if data.nom_equip==c.nom and data.mac_address==c.mac:
            client=c
            client.accepted=True
    return client

def ttl_registered(client):
    client.ttl_alive=client.ttl_alive+2
    time.sleep(3)
    client.ttl_alive=client.ttl_alive-1
    time.sleep(3)
    client.ttl_alive=client.ttl_alive-1

    if client.ttl_alive==0:
        client.estat="DISCONNECTED"
        debuger("Pasat temps maxim d'espera d'alive")
    return 0
    

def ttl_alive(client):
    client.ttl_alive=client.ttl_alive+3
    time.sleep(3)
    client.ttl_alive=client.ttl_alive-1
    time.sleep(3)
    client.ttl_alive=client.ttl_alive-1
    time.sleep(3)
    client.ttl_alive=client.ttl_alive-1

    if client.ttl_alive==0:
        client.estat="DISCONNECTED"
        debuger("Pasat temps maxim d'espera d'alive")
    return 0

def check_register(data, addr, client, server_config, sock):

    if client.accepted==True:
        if client.estat=="DISCONNECTED":
            if data.num_aleatori=="000000" and data.nom_equip==client.nom and data.mac_address==client.mac:
                debuger("Les dades concorden, enviant REGISTER_ACK")
                aleatori=0
                for i in range(0,6):
                    aleatori=(aleatori*10)+randint(0, 9)
                data=POINT(tipus_paquet=0x01, nom_equip=server_config["nom"], mac_address=server_config["mac"],  num_aleatori=str(aleatori), dades=server_config["tcp_port"])
                sock.sendto(data, addr)
                client.aleatori=str(aleatori)
                client.estat="REGISTERED"
                client.ip=addr[0]
                t=threading.Thread(target=ttl_registered, args=(client,))
                t.daemon=True
                t.start()
            else:
                debuger("Les dades no concorden, enviant REGISTER_NACK")
                data=POINT(tipus_paquet=0x02, nom_equip="0000000", mac_address="0000000000000",  num_aleatori="000000", dades="Error en eles dades")
                sock.sendto(data, addr)
        else:
            if data.num_aleatori==client.aleatori and data.nom_equip==client.nom and data.mac_address==client.mac and addr[0]==client.ip:
                debuger("Les dades concorden, enviant REGISTER_ACK")
                data=POINT(tipus_paquet=0x01, nom_equip=server_config["nom"], mac_address=server_config["mac"],  num_aleatori=str(client.aleatori), dades=server_config["tcp_port"])
                sock.sendto(data,addr)
            else:
                debuger("Les dades no concorden, enviant REGISTER_NACK")
                data=POINT(tipus_paquet=0x02, nom_equip="0000000", mac_address="0000000000000",  num_aleatori="000000", dades="Error en les dades")
                sock.sendto(data,addr)
    else:
        debuger("Client no valid")
        data=POINT(tipus_paquet=0x03, nom_equip="0000000", mac_address="0000000000000",  num_aleatori="000000", dades="Equip no autoritzat")
        sock.sendto(data,addr)
        client.estat="DISCONNECTED"


def check_alive(data, addr, client, server_config, sock):

    if client.estat=="ALIVE" or client.estat=="REGISTERED":
        if data.nom_equip==client.nom and data.mac_address==client.mac:
            if data.num_aleatori==client.aleatori and addr[0]==client.ip:
                debuger("Les dades concorden, enviant ALIVE_ACK")
                data=POINT(tipus_paquet=0x11, nom_equip=server_config["nom"], mac_address=server_config["mac"],  num_aleatori=client.aleatori, dades=server_config["tcp_port"])
                sock.sendto(data, addr)
                client.estat="ALIVE"
                t=threading.Thread(target=ttl_alive, args=(client,))
                t.daemon=True
                t.start()
            else:
                debuger("Les dades no concorden, enviant ALIVE_NACK")
                data=POINT(tipus_paquet=0x12, nom_equip="0000000", mac_address="0000000000000",  num_aleatori="000000", dades="Error en el nombre aleatori")
                sock.sendto(data,addr)
        else:
            data=POINT(tipus_paquet=0x13, nom_equip="0000000", mac_address="0000000000000",  num_aleatori="000000", dades="Equip no autoritzat")
            sock.sendto(data,addr)
            client.estat="DISCONNECTED"
    else:
        debuger("Client no autoritzat, enviant ALIVE_REJ")
        data=POINT(tipus_paquet=0x13, nom_equip="0000000", mac_address="0000000000000",  num_aleatori="000000", dades="Client no autoritzat")
        sock.sendto(data,addr)
        
def listen(clients):
    debuger("Escoltant Comandes")
    quit = False
    while not quit:
        s = raw_input()
        if s=="list":
            make_list(clients) 
        elif s=="quit":
            quit = True
        else:
            debuger("Comanda incorrecta")
    os._exit(1)
    
def make_list(clients):
    cap=("-Nom-", "-Mac-", "-Estat-", "-IP-", "-Aleatori-")
    print '{0:<0} {1:>11} {2:>15} {3:>15} {4:>20}'.format(*cap)
    for client in clients:
        if client.estat=="DISCONNECTED":
            row_client= (client.nom,client.mac,client.estat)
            print '{0:<0} {1:>15} {2:>15}'.format(*row_client)
        else:
            row_client= (client.nom,client.mac,client.estat,client.ip,client.aleatori)
            print '{0:<0} {1:>15} {2:>15} {3:>14} {4:>15}'.format(*row_client)
            

class POINT (Structure):
    _fields_ = [("tipus_paquet",c_ubyte),
                ("nom_equip",c_char*7),
                ("mac_address",c_char*13),
                ("num_aleatori",c_char*7),
                ("dades",c_char*50)]

class Client:
    def __init__(self,nom,mac,estat,aleatori,ip,accepted,ttl_alive):
        self.nom=nom
        self.mac=mac
        self.estat=estat
        self.aleatori=aleatori
        self.ip=ip
        self.accepted=accepted
        self.ttl_alive=ttl_alive


if __name__=='__main__':
    debuger("Inici del servidor")
    server_config=server_configuration()
    clients=get_clients_autoritzats()
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("localhost", int(server_config["udp_port"])))
    debuger("Socket creat")
    t_listen=threading.Thread(target=listen, args=(clients,))
    t_listen.daemon=True
    t_listen.start()
    while True:
        data_struct, addr = sock.recvfrom(78)
        data=data_treatment(data_struct)
        server_config=server_configuration()
        reply(data, addr, clients, server_config, sock)
        




# coding=utf-8
#enconding: utf-8

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

x=0
j=2
k=3
w=4

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
        client=Client(line.split()[0],line.split()[1],"DISCONNECTED","000000","",False,0,False)
        llista_clients.append(client)
        
    return llista_clients

def data_treatment(data_struct):
    data=POINT.from_buffer_copy(data_struct)
    return data

def data_treatment_tcp(data_struct):
    data=POINT_TCP.from_buffer_copy(data_struct)
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
    client=Client("0000000","0000000000000","DISCONNECTED","000000","",False,0, False)
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

class POINT_TCP (Structure):
    _fields_ = [("tipus_paquet",c_ubyte),
                ("nom_equip",c_char*7),
                ("mac_address",c_char*13),
                ("num_aleatori",c_char*7),
                ("dades",c_char*150)]

class Client:
    def __init__(self,nom,mac,estat,aleatori,ip,accepted,ttl_alive,tcp_active):
        self.nom=nom
        self.mac=mac
        self.estat=estat
        self.aleatori=aleatori
        self.ip=ip
        self.accepted=accepted
        self.ttl_alive=ttl_alive
        self.tcp_active=tcp_active


def reply_tcp(client_tcp,sock_tcp,clients,server_config,addr_tcp):
    #Agafem dades del canal TCP
    data_struct_tcp = client_tcp.recv(178)
    data_struct_tcp=data_treatment_tcp(data_struct_tcp)

    #Comprovem que sigui un client autoritzat
    client=check_client(data_struct_tcp,clients)

    if(client.accepted==True and client.tcp_active==False):
        client.tcp_active==True
        check_tcp_pdu(data_struct_tcp,client,sock_tcp,server_config,addr_tcp)

    else:
        debuger("Enviat SEND_NACK, aquest client ja té el port tcp actiu")
        data_send_tcp=POINT_TCP(tipus_paquet=0x22, nom_equip="", mac_address="0000000000000",  num_aleatori="000000", dades="Dades addiccional errònies")
        sock_tcp.send(data_send_tcp)
        sock_tcp.close()
        return 0
        

def check_tcp_pdu(data_struct_tcp,client,sock_tcp,server_config,addr_tcp):
    if(data_struct_tcp.tipus_paquet==0x20):
        check_send_conf(data_struct_tcp,client,sock_tcp,server_config,addr_tcp)
    if(data_struct_tcp.tipus_paquet==0x30):
        #check_get_conf(data,client,sock_tcp,server_config,addr_tcp)
        return 0

def check_send_conf(data_struct_tcp,client,sock_tcp,server_config,addr_tcp):
    if data_struct_tcp.nom_equip==client.nom and data_struct_tcp.mac_address==client.mac:
        if data_struct_tcp.num_aleatori==client.aleatori and addr_tcp[0]==client.ip:
            data_send_tcp=POINT_TCP(tipus_paquet=0x21, nom_equip=server_config["nom"], mac_address=server_config["mac"],  num_aleatori=client.aleatori, dades=client.nom+".cfg")
            sock_tcp.send(data_send_tcp)
            debuger("Enviat SEND_ACK")
            file_tcp = open(client.nom+".cfg", "a")
            while data.tipus_paquet!=0x25:
                ready = select.select([sock_tcp], [], [], w)
                if ready[0]:
                    data_recv_tcp=sock_tcp.recv(178)
                data_recv_tcp=data_treatment(data_recv_tcp)
                file_tcp.write(data_recv_tcp.dades)
            debuger("Enviats tots els SEND_DATA")
            file_tcp.close()
            sock_tcp.close()
            client.tcp_active==False
            return 0
        else:
            debuger("Dades addiccional errònies ")
            data_send_tcp=POINT_TCP(tipus_paquet=0x22, nom_equip="", mac_address="0000000000000",  num_aleatori="000000", dades="Dades addiccional errònies")
            sock_tcp.send(data_send_tcp)
            debuger("Enviat SEND_NACK")
            sock_tcp.close()
            client.tcp_active==False
            return 0
    else:
        debuger("Dades principals errònies")
        data_send_tcp=POINT_TCP(tipus_paquet=0x23, nom_equip="", mac_address="0000000000000",  num_aleatori="000000", dades="Dades principals errònies")
        sock_tcp.send(data_send_tcp)
        debuger("Enviar SEND_REJ")
        sock_tcp.close()
        client.tcp_active==False
        return 0

                

if __name__=='__main__':
    debuger("Inici del servidor")
    server_config=server_configuration()
    clients=get_clients_autoritzats()
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("localhost", int(server_config["udp_port"])))
    debuger("Socket UDP creat")
    sock_tcp = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock_tcp.bind(("localhost", int(server_config["tcp_port"])))
    listen=sock_tcp.listen(5)
    debuger("Socket TCP creat") 
    t_listen=threading.Thread(target=listen, args=(clients,))
    t_listen.daemon=True
    t_listen.start()
    while True:
        #UDP
        data_struct, addr = sock.recvfrom(78)
        data=data_treatment(data_struct)
        server_config=server_configuration()
        reply(data, addr, clients, server_config, sock)
        #TCP 
        prepared = select.select([sock_tcp], [], [], 0)
        if prepared[0]:
            client_tcp,addr_tcp = sock_tcp.accept()
            debuger("Creat fill per a gestio del port TCP")
            t_listen_tcp=threading.Thread(target=reply_tcp, args=(client_tcp,sock_tcp,clients,server_config,addr_tcp,))
            t_listen_tcp.daemon=True
            t_listen_tcp.start()

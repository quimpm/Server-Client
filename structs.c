
/*Estructura per les dades del fitxer de configuració del client*/
struct config_client{
    char nom[7];
    char mac[13];
    char id_servidor[50];
    int port_servidor;
};
/*Estructura PDU*/
struct server_register{
    unsigned char tipus_paquet;
    char nom_equip[7];
    char mac_adress[13];
    char num_aleatori[7];
    char dades[50];
};
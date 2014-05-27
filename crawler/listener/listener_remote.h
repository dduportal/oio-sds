#ifndef __LISTENER_REMOTE_H
#define __LISTENER_REMOTE_H



#include <zmq.h>
#include <json/json.h>
#include <glib.h> 


/******************************************************************************/
/* constantes                                                                 */
/******************************************************************************/
#define LISTENER_JSON_KEYNAME_HEAD_NAME_command        "command"
#define LISTENER_JSON_KEYNAME_HEAD_STATUS_setcrawlerid "setcrawlerid"
#define LISTENER_JSON_KEYNAME_HEAD_STATUS_stopact      "stopact"
#define LISTENER_JSON_KEYNAME_HEAD_STATUS_data         ""


/**
 *  * Container created if doesn't exist on the namespace
 *   * contained all file generated for each crawler action reduce
 *    */
#define LISTENER_RESULT_CONTAINER_NAME      "__ListenerResult__"


/**
 *  * the file which contained all result generated by action
 *   * format: <YYYYMMDDhhmmss>_<action name>_[listcopntainer]_<action pid>
 *    */
//#define LISTENER__RESULT_CONTENT_NAME        "%s_%s_%s"





/******************************************************************************/
/* structure                                                                  */
/******************************************************************************/
typedef struct {
	int code;
	char* message;
} TLstError;

TLstError* listener_remote_error_new(int code, char* format, ...);
void       listener_remote_error_clean(TLstError* err);



/******************************************************************************/
/* Functions                                                                  */
/******************************************************************************/


// contexte management
void* listener_remote_init(void);
void  listener_remote_close(void* zmq_ctx, void* zmq_sock);

//socket management
void* listener_remote_connect(TLstError **err, void* zmq_ctx, const char* listenerUrl, 
								int linger, int64_t hwm);
TLstError* listener_remote_sendBuffer(void* zmq_sock, const char* buf, int buflen);
TLstError* listener_remote_sendJSON(void* zmq_sock, json_object* j_root);
void listener_remote_closesocket(void* zmq_sock);



/******************************************************************************/
/* JSON management                                                             */
/******************************************************************************/
typedef enum {
	LST_SECTION_HEAD=0,
	LST_SECTION_DATAH,
	LST_SECTION_DATAR,

	LST_SECTION_max
}ELstSection;




#define LSTJSON_STATUS_MAX_CARACT  40
#define LSTJSON_IDCRAWL_MAX_CARACT 40

/**
 *  * \brief Header of each message, before data in gchar* traditionnaly
 *   */
typedef struct {
    gchar* action_name;                       //nom de l'action originaire de la requete
    int    action_pid;                        //pid
    gchar  status[LSTJSON_STATUS_MAX_CARACT];    //reserved
    int    idmsg;                             // [0..N[ increase each request sending
    gchar  idcrawl[LSTJSON_IDCRAWL_MAX_CARACT];  //id du crawler
} TLstJSONHeader;



struct json_object * listener_remote_json_init(TLstJSONHeader* msgH, unsigned char bAllDataSection);
TLstError*           listener_remote_json_addStringToSection(json_object* j_section, char* key, char* value);
TLstError*           listener_remote_json_addIntToSection(   json_object* j_section, char* key, int value);
TLstError*              listener_remote_json_addSection(        json_object* j_root, ELstSection section, json_object* j_section);
json_object*         listener_remote_json_newSection(        void);
void                 listener_remote_json_clean(             json_object* j_obj);

char* listener_remote_json_getStr(json_object* object);



#endif //__LISTENER_REMOTE_H



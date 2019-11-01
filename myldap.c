#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ldap.h>
#include "myldap.h"

#define LDAP_URI "ldap://ldap.technikum-wien.at:389"
#define SEARCHBASE "ou=People,dc=technikum-wien,dc=at" // achtung was wenn sich struktur ändert;nicht ideal
#define SCOPE LDAP_SCOPE_SUBTREE

//TODO: Threads, ev list durch read/del triggern?

int anonymUserSearch(char *username){
    LDAP *ld;			/* LDAP resource handle */
    LDAPMessage *result;	/* LDAP result handle; gesamtes ergebnis der suche */

    BerValue *servercredp; //siehe oben def
    BerValue cred;
    cred.bv_val = "";
    cred.bv_len = 0;
    int rc=0;
    char filter[40] = "";
    int resultCount = 0;

    const char *attribs[] = { "uid", "cn", NULL };		/* attribute array for search; muss nullterminiert sein */

    int ldapversion = LDAP_VERSION3;//bei 2 keine verschlüsselung; korrekte initialisierung wichtig!

    /* 1. schritt connection aufbauen;setup LDAP connection */
    if (ldap_initialize(&ld,LDAP_URI) != LDAP_SUCCESS) //hat geklappt
    {
        fprintf(stderr,"ldap_init failed"); //sonst err
        return EXIT_FAILURE;
    }

    printf("connected to LDAP server %s\n",LDAP_URI);

    if ((rc = ldap_set_option(ld, LDAP_OPT_PROTOCOL_VERSION, &ldapversion)) != LDAP_SUCCESS) //protokoll versionsnr muss stimmen sonst
    {
        fprintf(stderr, "ldap_set_option(PROTOCOL_VERSION): %s\n", ldap_err2string(rc));
        ldap_unbind_ext_s(ld, NULL, NULL); //wieder raus
        return EXIT_FAILURE;
    }

    //man pages checken für parameter, rückgabewerte
    if ((rc = ldap_start_tls_s(ld, NULL, NULL)) != LDAP_SUCCESS)
    {
        fprintf(stderr, "ldap_start_tls_s(): %s\n", ldap_err2string(rc));
        ldap_unbind_ext_s(ld, NULL, NULL);
        return EXIT_FAILURE;
    }

    /* anonymous bind */
    rc = ldap_sasl_bind_s(ld,"",LDAP_SASL_SIMPLE,&cred,NULL,NULL,&servercredp);

    if (rc != LDAP_SUCCESS)
    {
        fprintf(stderr,"LDAP bind error: %s\n",ldap_err2string(rc));
        ldap_unbind_ext_s(ld, NULL, NULL);
        return EXIT_FAILURE;
    }
    else
    {
        printf("bind successful\n");
    }

    sprintf(filter, "(uid=%s)", username);
    /* perform ldap search */
    //printf("filter: %s\n", filter);
    rc = ldap_search_ext_s(ld, SEARCHBASE, SCOPE, filter, (char **)attribs, 0, NULL, NULL, NULL, 500, &result);

    if (rc != LDAP_SUCCESS)
    {
        fprintf(stderr,"LDAP search error: %s\n",ldap_err2string(rc));
        ldap_unbind_ext_s(ld, NULL, NULL);
        return EXIT_FAILURE;
    }

    resultCount = ldap_count_entries(ld, result);
    //printf("Total results: %d\n", resultCount);

    ldap_msgfree(result);
    printf("LDAP search succeeded\n");

    ldap_unbind_ext_s(ld, NULL, NULL);
    if (resultCount == 1) {
        return EXIT_SUCCESS;
    } else {
        return EXIT_FAILURE;
    }

}

int passwordCheck(char *username, char *password){
    //printf("Passwort: %s\n", password);
    LDAP *ld;			/* LDAP resource handle */
    BerValue *servercredp; //siehe oben def
    BerValue cred;
    cred.bv_val = password;
    cred.bv_len = strlen(password);
    int rc=0;

    char filter[40] = "";

    int ldapversion = LDAP_VERSION3;//bei 2 keine verschlüsselung; korrekte initialisierung wichtig!

    /* 1. schritt connection aufbauen;setup LDAP connection */
    if (ldap_initialize(&ld,LDAP_URI) != LDAP_SUCCESS) //hat geklappt
    {
        fprintf(stderr,"ldap_init failed"); //sonst err
        return EXIT_FAILURE;
    }

    //printf("connected to LDAP server %s\n",LDAP_URI);

    if ((rc = ldap_set_option(ld, LDAP_OPT_PROTOCOL_VERSION, &ldapversion)) != LDAP_SUCCESS) //protokoll versionsnr muss stimmen sonst
    {
        fprintf(stderr, "ldap_set_option(PROTOCOL_VERSION): %s\n", ldap_err2string(rc));
        ldap_unbind_ext_s(ld, NULL, NULL); //wieder raus
        return EXIT_FAILURE;
    }

    //man pages checken für parameter, rückgabewerte
    if ((rc = ldap_start_tls_s(ld, NULL, NULL)) != LDAP_SUCCESS)
    {
        fprintf(stderr, "ldap_start_tls_s(): %s\n", ldap_err2string(rc));
        ldap_unbind_ext_s(ld, NULL, NULL);
        return EXIT_FAILURE;
    }

    /* user bind */
    char binduser [60] = "";
    sprintf(binduser, "uid=%s,ou=People,dc=technikum-wien,dc=at", username);
    //printf("binduser: %s\n", binduser);
    //printf("size of pw: %ld\n", cred.bv_len);
    //printf("cred: %s\n", cred);
    rc = ldap_sasl_bind_s(ld,binduser,LDAP_SASL_SIMPLE,&cred,NULL,NULL,&servercredp);

    if (rc != LDAP_SUCCESS)
    {
        fprintf(stderr,"LDAP bind error: %s\n",ldap_err2string(rc));
        ldap_unbind_ext_s(ld, NULL, NULL);
        return EXIT_FAILURE;
    }
    else
    {
        printf("bind successful\n");
    }

    sprintf(filter, "(uid=%s)", username);
    ldap_unbind_ext_s(ld, NULL, NULL);
    return EXIT_SUCCESS;
}

int ldapLogin(char *username, char *password)
{
    int status = 0;
    status = anonymUserSearch(username);
    //printf("status anonym: %d\n", status);
    if (status != 0){
        return status;
    }
    return passwordCheck(username, password);
}

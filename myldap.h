#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ldap.h>

#define LDAP_URI "ldap://ldap.technikum-wien.at:389"
#define SEARCHBASE "ou=People,dc=technikum-wien,dc=at" // achtung was wenn sich struktur ändert; nicht ideal
#define SCOPE LDAP_SCOPE_SUBTREE
#define MAX_FILTER_LEN 15

int anonymous_user_search(char *username);

int ldap_login(char *username, char *password);

int password_check(char *username, char *password);
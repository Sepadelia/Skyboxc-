// gen_icon.cpp — Génère Skybox.ico depuis icon_data.h (build step)
#include <cstdio>
#include "icon_data.h"
int main(){
    FILE*f=fopen("Skybox.ico","wb");
    if(!f){puts("Erreur: impossible d'écrire Skybox.ico");return 1;}
    fwrite(g_icon_data,1,g_icon_size,f);
    fclose(f);
    printf("Skybox.ico regenere (%u bytes)\n",g_icon_size);
    return 0;
}

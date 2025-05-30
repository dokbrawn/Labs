#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define MAX_NOF_MIB 64

typedef enum scs {
    scs15or60 = 0,
    scs30or120
} scs_e;

// Упаковываем структуру, чтобы избежать выравнивания
#pragma pack(push, 1)
typedef struct MIB {
    uint8_t systemFrameNumber : 6;          // 6 бит
    scs_e subCarrierSpacingCommon : 1;      // 1 бит
    uint8_t ssb_SubcarrierOffset : 4;       // 4 бита
    uint8_t dmrs_TypeA_Position : 1;        // 1 бит
    uint8_t pdcch_ConfigSIB1 : 8;           // 8 бит
    uint8_t cellBarred : 1;                 // 1 бит
    uint8_t intraFreqReselection : 1;       // 1 бит
    uint8_t spare : 1;                      // 1 бит
} MIB_t;
#pragma pack(pop)

typedef struct MIB_LIST {
    int nof_mibs;
    MIB_t mib[MAX_NOF_MIB];
} MIB_LIST_t;

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s bin_file\n", argv[0]);
        exit(1);
    }

    MIB_LIST_t *mib_list = (MIB_LIST_t *)malloc(sizeof(MIB_LIST_t));
    if (mib_list == NULL) {
        perror("Ошибка выделения памяти");
        exit(1);
    }

    FILE *file = fopen(argv[1], "rb");
    if (file == NULL) {
        perror("Ошибка открытия файла");
        free(mib_list);
        exit(1);
    }

   
    printf("Размер MIB_t: %zu байт\n", sizeof(MIB_t)); 

    mib_list->nof_mibs = fread(mib_list->mib, sizeof(MIB_t), MAX_NOF_MIB, file);
    if (mib_list->nof_mibs <= 0) {
        printf("Файл пуст или ошибка чтения\n");
        fclose(file);
        free(mib_list);
        exit(1);
    }

    printf("nof_mibs: %d\n", mib_list->nof_mibs);
    for (int i = 0; i < mib_list->nof_mibs; i++) {
        printf("MIB %d:\n", i + 1);
        printf("  systemFrameNumber: %u\n", mib_list->mib[i].systemFrameNumber);
        printf("  subCarrierSpacingCommon: %s\n", 
               mib_list->mib[i].subCarrierSpacingCommon == scs15or60 ? "scs15or60" : "scs30or120");
        printf("  ssb_SubcarrierOffset: %u\n", mib_list->mib[i].ssb_SubcarrierOffset);
        printf("  dmrs_TypeA_Position: %s\n", 
               mib_list->mib[i].dmrs_TypeA_Position == 0 ? "pos2" : "pos3");
        printf("  pdcch_ConfigSIB1: %u\n", mib_list->mib[i].pdcch_ConfigSIB1);
        printf("  cellBarred: %s\n", 
               mib_list->mib[i].cellBarred == 0 ? "notBarred" : "barred");
        printf("  intraFreqReselection: %s\n", 
               mib_list->mib[i].intraFreqReselection == 0 ? "allowed" : "notAllowed");
        printf("  spare: %u\n", mib_list->mib[i].spare);
        printf("\n");
    }

    fclose(file);
    free(mib_list);

    return 0;
}
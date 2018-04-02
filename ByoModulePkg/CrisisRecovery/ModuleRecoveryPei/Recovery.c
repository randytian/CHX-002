/** @file

Copyright (c) 2006 - 2011, Byosoft Corporation.<BR>
All rights reserved.This software and associated documentation (if any)
is furnished under a license and may only be used or copied in
accordance with the terms of the license. Except as permitted by such
license, no part of this software or documentation may be reproduced,
stored in a retrieval system, or transmitted in any form or by any
means without the express written consent of Byosoft Corporation.

File Name:
  Recovery.c

Abstract:


Revision History:

Bug 2909:   Add some port 80 status codes into EDKII code.
TIME:       2011-09-23
$AUTHOR:    Liu Chunling
$REVIEWERS:
$SCOPE:     All Platforms
$TECHNICAL:
  1. Improve Port80 map table.
  2. Add Port80 status codes in the corresponding position to report status code.
  3. Change the seconed REPORT_STATUS_CODE_WITH_EXTENDED_DATA macro's parameter
     to EFI_SW_PC_INIT_END from EFI_SW_PC_INIT_BEGIN.
$END--------------------------------------------------------------------

**/
/** @file

Copyright (c) 2006 - 2011, BYOSoft Corporation. All rights
reserved.<BR> This software and associated documentation (if
any) is furnished under a license and may only be used or copied
in accordance with the terms of the license. Except as permitted
by such license, no part of this software or documentation may
be reproduced, stored in a retrieval system, or transmitted in
any form or by any means without the express written consent of
BYOSoft Corporation.

**/

//#include "Recovery.h"
#include <Guid\CapsuleRecord.h>
#include <Pi\PiHob.h>
#include <Pi\PiPeiCis.h>
#include <Pi\PiFirmwareFile.h>
#include <Ppi\FirmwareVolume.h>
#include <Library\BaseMemoryLib.h>
#include <Ppi\RecoveryModule.h>
#include <Ppi\DeviceRecoveryModule.h>
#include <Uefi\UefiBaseType.h>
#include <Guid\BiosId.h>
#include <Library\PeiServicesTablePointerLib.h>
#include <Library\PcdLib.h>
#include <Library\DebugLib.h>
#include <Library\HobLib.h>
#include <Library\PeiServicesLib.h>
#include <Library\ReportStatusCodeLib.h>
#include <Guid\FirmwareFileSystem2.h>
#include <Library\DebugLib.h>
#include <Library\MemoryAllocationLib.h>

CAPSULE_RECORD  mCapsuleLoaded;

EFI_STATUS
EFIAPI
PlatformRecoveryModule (
  IN EFI_PEI_SERVICES                       **PeiServices,
  IN EFI_PEI_RECOVERY_MODULE_PPI            *This
  );

static EFI_PEI_RECOVERY_MODULE_PPI mRecoveryPpi = {
  PlatformRecoveryModule
};

static EFI_PEI_PPI_DESCRIPTOR mRecoveryPpiList = {
    EFI_PEI_PPI_DESCRIPTOR_PPI | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST,
    &gEfiPeiRecoveryModulePpiGuid,
    &mRecoveryPpi
  };

/**
  This is a internal function to get bios ID from a given BIOS image

  @param CapsuleBaseAddress     A pointer to the base address of a BIOS image loaded in
                                memory
  @param CapsuleLength          The length of the capsule volume
  @param BiosIdImage            Return a information about the loaded bios
                                image

  @return EFI_SUCCESS           Get BiosIdImage successfully
  @return EFI_NOT_FOUND         Fail to get BiosIdImage

**/
EFI_STATUS
GetRecoveryCapsuleBiosIdImage (
  IN UINTN                CapsuleBaseAddress,
  IN OUT BIOS_ID_IMAGE    *BiosIdImage
  )
{
  EFI_STATUS                    Status;
  EFI_PEI_FILE_HANDLE           FileHandle;
  EFI_PEI_PPI_DESCRIPTOR        *PpiDescriptor;
  EFI_PEI_FIRMWARE_VOLUME_PPI   *Ffs2FvPpi;
  VOID                          *SectionData;
  UINTN                         FvMainBaseAddress;
  EFI_FIRMWARE_VOLUME_HEADER    *FvHeader;
  UINT64                        FvLength;
  VOID                          *NewFvBuffer;
  UINT32                        FvAlignment;

  FileHandle = NULL;

  FvMainBaseAddress = CapsuleBaseAddress + (PcdGet32(PcdFlashFvMainBase) - PcdGet32(PcdFlashAreaBaseAddress));

// First find it at top FV.
  Status = PeiServicesLocatePpi (
              &gEfiFirmwareFileSystem2Guid,
              0,
              &PpiDescriptor,
              &Ffs2FvPpi
              );
  ASSERT(!EFI_ERROR(Status));

  if(FeaturePcdGet(PcdRecoveryFindBiosIdFirstTryTopFv)){
    FvHeader = (EFI_FIRMWARE_VOLUME_HEADER*)FvMainBaseAddress;
    Status = Ffs2FvPpi->FindFileByName (
                          Ffs2FvPpi,
                          &gEfiBiosIdGuid,
                          (EFI_PEI_FV_HANDLE*)&FvHeader,
                          &FileHandle
                          );
    if (!EFI_ERROR(Status)) {
      Status = Ffs2FvPpi->FindSectionByType (
                            Ffs2FvPpi,
                            EFI_SECTION_RAW,
                            FileHandle,
                            &SectionData
                            );
      if (!EFI_ERROR(Status)) {
        CopyMem (BiosIdImage, SectionData, sizeof(BIOS_ID_IMAGE));
        return EFI_SUCCESS;
      }
    } 
  }

// Second, find it at FV under FV.
  Status = Ffs2FvPpi->FindFileByType (
                        Ffs2FvPpi,
                        EFI_FV_FILETYPE_FIRMWARE_VOLUME_IMAGE,
                        (EFI_PEI_FV_HANDLE)FvMainBaseAddress,
                        &FileHandle
                        );
  if (EFI_ERROR(Status)) {
    return Status;
  }
  //
  // Extract the compressed fv image
  //
  Status = Ffs2FvPpi->FindSectionByType (
                        Ffs2FvPpi,
                        EFI_SECTION_FIRMWARE_VOLUME_IMAGE,
                        FileHandle,
                        &FvHeader
                        );
  if (EFI_ERROR(Status)) {
    return Status;
  }


  //
  // FvAlignment must be more than 8 bytes required by FvHeader structure.
  //
  FvAlignment = 1 << ((ReadUnaligned32 (&FvHeader->Attributes) & EFI_FVB2_ALIGNMENT) >> 16);
  if (FvAlignment < 8) {
    FvAlignment = 8;
  }

  //
  // Check FvImage
  //
  if ((UINTN) FvHeader % FvAlignment != 0) {
    FvLength    = ReadUnaligned64 (&FvHeader->FvLength);
    NewFvBuffer = AllocateAlignedPages (EFI_SIZE_TO_PAGES ((UINT32) FvLength), FvAlignment);
    if (NewFvBuffer == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }
    CopyMem (NewFvBuffer, FvHeader, (UINTN) FvLength);
    FvHeader = (EFI_FIRMWARE_VOLUME_HEADER*) NewFvBuffer;
  }


  //
  // Fv volume has alreadly extracted.
  // Find BIOSID
  //
  Status = Ffs2FvPpi->FindFileByName (
                        Ffs2FvPpi,
                        &gEfiBiosIdGuid,
                        (EFI_PEI_FV_HANDLE *)&FvHeader,
                        &FileHandle
                        );
  if (!EFI_ERROR(Status)) {
    Status = Ffs2FvPpi->FindSectionByType (
                          Ffs2FvPpi,
                          EFI_SECTION_RAW,
                          FileHandle,
                          &SectionData
                          );
    if (!EFI_ERROR(Status)) {
      CopyMem (BiosIdImage, SectionData, sizeof(BIOS_ID_IMAGE));
      return EFI_SUCCESS;
    }
  }

  return EFI_NOT_FOUND;
}

/**
  This internal function is to compare between with two CAPSULE_INFO
  strcture, and return the one who has newer version.

  @param CapsuleInfo1           a strcture contian capsule infomation.
  @param CapsuleInfo2           a strcture contian capsule infomation.

  @return CAPSULE_INFO          the one who has the newer version.

**/
BOOLEAN
CompareCapsuleVersion (
  IN CAPSULE_INFO       *CapsuleInfo1,
  IN CAPSULE_INFO       *CapsuleInfo2
  )
{
  UINT8           Index;

  //
  // Compare major version, and return the new version cpasule
  //
  for (Index = 0; Index < sizeof(CapsuleInfo1->BiosIdImage.BiosIdString.VersionMajor) / sizeof(CHAR16); Index++) {
    if (CapsuleInfo1->BiosIdImage.BiosIdString.VersionMajor[Index] > CapsuleInfo2->BiosIdImage.BiosIdString.VersionMajor[Index]) {
      return TRUE;
    } else if (CapsuleInfo1->BiosIdImage.BiosIdString.VersionMajor[Index] < CapsuleInfo2->BiosIdImage.BiosIdString.VersionMajor[Index]) {
      return FALSE;
    }
  }

  //
  // if they have a same major version, compare timestamp
  //
  for (Index = 0; Index < sizeof(CapsuleInfo1->BiosIdImage.BiosIdString.TimeStamp) / sizeof(CHAR16); Index++) {
    if (CapsuleInfo1->BiosIdImage.BiosIdString.TimeStamp[Index] > CapsuleInfo2->BiosIdImage.BiosIdString.TimeStamp[Index]) {
      return TRUE;
    } else if (CapsuleInfo1->BiosIdImage.BiosIdString.TimeStamp[Index] < CapsuleInfo2->BiosIdImage.BiosIdString.TimeStamp[Index]) {
      return FALSE;
    }
  }

  //
  // if they have same major version and timestamp, return the first one
  //
  return FALSE;
}

/**
  This funtion is to select a best suit capsule volume in a capsule record
  for DXE boot, and the index value.

  @param CapsuleRecord          a strcture records capsule volume
                                infomation.

  @return UINT8                 the suitable index in the record

**/
UINT8
PickUpCapsuleByVersionAndBuildTime (
  IN CAPSULE_RECORD       *CapsuleRecord
  )
{
  UINT8               Index1;
  UINT8               Index2;
  
  if (CapsuleRecord->CapsuleCount == 1) {
    return 0;
  }

  //
  // compare to get the capsuleInfo which have the newest version
  //
  Index1 = 0;
  for (Index2 = 1; Index2 < CapsuleRecord->CapsuleCount; Index2++) {
    if (CompareCapsuleVersion (
          &CapsuleRecord->CapsuleInfo[Index2], 
          &CapsuleRecord->CapsuleInfo[Index1]
          )) {
      Index1 = Index2;
    }
  }
    
  return Index1;
}


/**
  Provide the functionality of the Recovery Module.

  @param  PeiServices           General purpose services available to
                                every PEIM.
  @param  This                  Pointer to recovery module ppi

  @return  EFI_SUCCESS          recovery successfully

**/
EFI_STATUS
EFIAPI
PlatformRecoveryModule (
  IN EFI_PEI_SERVICES                       **PeiServices,
  IN EFI_PEI_RECOVERY_MODULE_PPI            *This
  )
{
  EFI_STATUS                             Status;
  EFI_PEI_DEVICE_RECOVERY_MODULE_PPI     *DeviceRecoveryModule;
  UINTN                                  NumberOfImageProviders;
  BOOLEAN                                ProviderAvailable;
  UINTN                                  NumberRecoveryCapsules;
  UINTN                                  RecoveryCapsuleSize;
  EFI_GUID                               DeviceId;
  BOOLEAN                                ImageFound;
  EFI_PHYSICAL_ADDRESS                   Address;
  VOID                                   *Buffer;
  EFI_PEI_HOB_POINTERS                   Hob;
  BOOLEAN                                HobUpdate;

  EFI_HOB_UEFI_CAPSULE                   *CapsuleVolumeHob;
  BIOS_ID_IMAGE                          BiosIdImage;
  CHAR16                                 BuildType;
  BIOS_ID_IMAGE                          BiosIdImageInFlash;
  EFI_PHYSICAL_ADDRESS                   FvMainBase;
  UINT64                                 FvMainSize;
  UINT8                                  SelectIndex;
  UINT8                                  Index;

  Status = EFI_SUCCESS;
  HobUpdate = FALSE;

  ProviderAvailable = TRUE;
  ImageFound        = FALSE;
  NumberOfImageProviders = 0;

  DeviceRecoveryModule = NULL;
  RecoveryCapsuleSize = 0;
  Buffer = NULL;

  REPORT_STATUS_CODE (
    EFI_PROGRESS_CODE,
    (EFI_SOFTWARE_PEI_MODULE | EFI_SW_PEI_PC_RECOVERY_BEGIN)
    );

  //
  // get BIOSID from Recovery FV in flash part
  //
  BuildType = L'R';
  Status = GetBiosId(&BiosIdImageInFlash);
  ASSERT (Status == EFI_SUCCESS);
  BuildType = BiosIdImageInFlash.BiosIdString.BuildType;

  do {
    Status = (*PeiServices)->LocatePpi (
                               PeiServices,
                               &gEfiPeiDeviceRecoveryModulePpiGuid,
                               NumberOfImageProviders,
                               NULL,
                               &DeviceRecoveryModule
                               );
    if (!EFI_ERROR(Status)) {
      NumberOfImageProviders++;
      Status = DeviceRecoveryModule->GetNumberRecoveryCapsules (
                                        PeiServices,
                                        DeviceRecoveryModule,
                                        &NumberRecoveryCapsules
                                        );
      DEBUG ((EFI_D_INFO, "NumberRecoveryCapsules:%d\n", NumberRecoveryCapsules));			
      if (NumberRecoveryCapsules != 0) {
        REPORT_STATUS_CODE (
          EFI_PROGRESS_CODE,
          (EFI_SOFTWARE_PEI_MODULE | EFI_SW_PEI_PC_CAPSULE_LOAD)
          );
        
        for (Index = 1; Index <= NumberRecoveryCapsules; Index++) {
          //
          // Only load not more than MAX_CAPUSLE_NUMBER images
          //
          if (mCapsuleLoaded.CapsuleCount < MAX_CAPUSLE_NUMBER) {
            Status = DeviceRecoveryModule->GetRecoveryCapsuleInfo (
                                              PeiServices,
                                              DeviceRecoveryModule,
                                              Index,
                                              &RecoveryCapsuleSize,
                                              &DeviceId
                                              );
            if (EFI_ERROR(Status)) {
              DEBUG ((EFI_D_ERROR, "GetRecoveryCapsuleInfo:%r\n", Status));								
              REPORT_STATUS_CODE (
                EFI_ERROR_CODE,
                (EFI_SOFTWARE_PEI_MODULE | EFI_SW_PEI_EC_INVALID_CAPSULE_DESCRIPTOR)
                );
              return Status;
            }

            Status = (*PeiServices)->AllocatePages (
                        PeiServices,
                        EfiBootServicesCode,
                        (RecoveryCapsuleSize - 1) / 0x1000 + 1,
                        &Address
                        );

            if (EFI_ERROR(Status)) {
              DEBUG ((EFI_D_ERROR, "AllocatePages:%r\n", Status));								
              return Status;
            }

            Buffer = (VOID *)(UINTN)Address;

            Status = DeviceRecoveryModule->LoadRecoveryCapsule (
                                              PeiServices,
                                              DeviceRecoveryModule,
                                              Index,
                                              Buffer
                                              );
            if (EFI_ERROR (Status)) {
              DEBUG ((EFI_D_ERROR, "LoadRecoveryCapsule:%r\n", Status));								
              REPORT_STATUS_CODE (
                EFI_ERROR_CODE,
                (EFI_SOFTWARE_PEI_MODULE | EFI_SW_PEI_EC_INVALID_CAPSULE_DESCRIPTOR)
                );
              return Status;
            }

            Status = GetRecoveryCapsuleBiosIdImage((UINTN)Buffer, &BiosIdImage);
            if (EFI_ERROR(Status)) {
              DEBUG ((EFI_D_ERROR, "GetRecoveryCapsuleBiosIdImage:%r\n", Status));								
              REPORT_STATUS_CODE (
                EFI_ERROR_CODE,
                (EFI_SOFTWARE_PEI_MODULE | EFI_SW_PEI_EC_INVALID_CAPSULE_DESCRIPTOR)
                );
              continue;
            }
            //
            // Only get the image with the same build type
            //
            if (BiosIdImage.BiosIdString.BuildType != BuildType) {
              DEBUG ((EFI_D_INFO, "%s\n", &BiosIdImage));
              DEBUG ((EFI_D_INFO, "This recovery image build type not match. Discard!\n"));
              REPORT_STATUS_CODE (
                EFI_ERROR_CODE,
                (EFI_SOFTWARE_PEI_MODULE | EFI_SW_PEI_EC_INVALID_CAPSULE_DESCRIPTOR)
                );
              continue;
            }
            //
            // Compare BIOS ID
            //
            if (StrnCmp (BiosIdImageInFlash.BiosIdString.BoardId, 
                         BiosIdImage.BiosIdString.BoardId, 
                         sizeof(BiosIdImage.BiosIdString.BoardId))
               ) {
              DEBUG ((EFI_D_INFO, "%s\n", &BiosIdImage));
              DEBUG ((EFI_D_INFO, "This recovery image BIOSID not match. Discard!\n"));
              REPORT_STATUS_CODE (
                EFI_ERROR_CODE,
                (EFI_SOFTWARE_PEI_MODULE | EFI_SW_PEI_EC_INVALID_CAPSULE_DESCRIPTOR)
                );
              continue;
            }
            //
            // Add this recovery capsule into mRecoveryCapsuleLoaded
            //
            mCapsuleLoaded.CapsuleInfo[mCapsuleLoaded.CapsuleCount].BaseAddress = (EFI_PHYSICAL_ADDRESS)(UINTN)Buffer;
            mCapsuleLoaded.CapsuleInfo[mCapsuleLoaded.CapsuleCount].Length = (UINT64)RecoveryCapsuleSize;
            CopyMem (&(mCapsuleLoaded.CapsuleInfo[mCapsuleLoaded.CapsuleCount].BiosIdImage), &BiosIdImage, sizeof(BiosIdImage));
            mCapsuleLoaded.CapsuleInfo[mCapsuleLoaded.CapsuleCount].DeviceId = DeviceId;
            mCapsuleLoaded.CapsuleInfo[mCapsuleLoaded.CapsuleCount].Actived = FALSE;
            mCapsuleLoaded.CapsuleCount ++;
            //
            // Build CV Hob
            //
            Status = (*PeiServices)->CreateHob (
                                       PeiServices,
                                       EFI_HOB_TYPE_UEFI_CAPSULE,
                                       sizeof (EFI_HOB_UEFI_CAPSULE),
                                       (VOID **) &CapsuleVolumeHob
                                       );
            ASSERT (Status == EFI_SUCCESS);
            CapsuleVolumeHob->BaseAddress = (EFI_PHYSICAL_ADDRESS)Buffer;
            CapsuleVolumeHob->Length = (UINT64)RecoveryCapsuleSize;
          } else {
            ProviderAvailable = FALSE;
            break;
          }
        }
      }
    } else {
      ProviderAvailable = FALSE;
    }

  } while (ProviderAvailable == TRUE);

  //
  // We get the capsule volume
  //
  if (mCapsuleLoaded.CapsuleCount > 0) {
    DEBUG ((EFI_D_INFO, "Find recovery %d capsule:\n", mCapsuleLoaded.CapsuleCount));
    //
    // select the best one for DXE to continue system boot
    //
    SelectIndex = PickUpCapsuleByVersionAndBuildTime (&mCapsuleLoaded);
    mCapsuleLoaded.CapsuleInfo[SelectIndex].Actived = TRUE;

    //
    // Copy to extent guid hob
    //
    Status = (*PeiServices)->CreateHob (
                              PeiServices,
                              EFI_HOB_TYPE_GUID_EXTENSION,
                              sizeof(EFI_HOB_GUID_TYPE) + sizeof(CAPSULE_RECORD),
                              (VOID **)&Hob.Guid
                              );
    ASSERT (Status == EFI_SUCCESS);

    CopyGuid (&Hob.Guid->Name, &gRecoveryCapsuleRecordGuid);
    Hob.Guid ++;
    CopyMem ((CAPSULE_RECORD *)Hob.Guid, &mCapsuleLoaded, sizeof(CAPSULE_RECORD));
    //
    // Fv.FvMain to Hob
    //
    FvMainBase = mCapsuleLoaded.CapsuleInfo[SelectIndex].BaseAddress;
    FvMainBase += (PcdGet32(PcdFlashFvMainBase) - PcdGet32(PcdFlashAreaBaseAddress));
    FvMainSize = PcdGet32(PcdFlashFvMainSize);

    PeiServicesInstallFvInfoPpi (
      &(((EFI_FIRMWARE_VOLUME_HEADER *)(UINTN)FvMainBase)->FileSystemGuid),
      (VOID *)(UINTN)FvMainBase,
      (UINT32)FvMainSize,
      NULL,
      NULL
      );
    
    REPORT_STATUS_CODE (
      EFI_PROGRESS_CODE,
      (EFI_SOFTWARE_PEI_MODULE | EFI_SW_PEI_PC_CAPSULE_LOAD)
      );
  } else {
    DEBUG ((EFI_D_ERROR, "Pei Recovery not find recovery capsule!\n"));
    REPORT_STATUS_CODE (
      EFI_ERROR_CODE,
      (EFI_SOFTWARE_PEI_MODULE | EFI_SW_PEI_EC_NO_RECOVERY_CAPSULE)
      );
    return EFI_NOT_FOUND;
  }

  return EFI_SUCCESS;
}

/**
  Install Recovery PPI.

  @param FileHandle             Handle of the file being invoked.
  @param PeiServices            Describes the list of possible PEI Services.

  @return EFI_STATUS EFIAPI

**/
EFI_STATUS
EFIAPI
ModuleRecoveryPeimEntry (
  IN EFI_PEI_FILE_HANDLE        FileHandle,
  IN CONST EFI_PEI_SERVICES     **PeiServices
  )
{
  EFI_STATUS                      Status;
  EFI_PEI_HOB_POINTERS            Hob;
  EFI_BOOT_MODE                   BootMode;
  EFI_PEI_PPI_DESCRIPTOR          *RecoveryModuleDescriptor;
  EFI_PEI_RECOVERY_MODULE_PPI     *RecoveryModulePpi;

  BootMode = BOOT_WITH_FULL_CONFIGURATION;
  ZeroMem (&mCapsuleLoaded, sizeof(CAPSULE_RECORD));

  Status = (*PeiServices)->GetHobList (PeiServices, &Hob.Raw);
  if (EFI_ERROR(Status)) {
    return Status;
  }

  while (!END_OF_HOB_LIST (Hob)) {
    if (Hob.Header->HobType == EFI_HOB_TYPE_HANDOFF) {
      BootMode = Hob.HandoffInformationTable->BootMode;
    }
    Hob.Raw = GET_NEXT_HOB (Hob);
  }

  if (BootMode == BOOT_IN_RECOVERY_MODE) {
    Status = (*PeiServices)->LocatePpi (
                               PeiServices,
                               &gEfiPeiRecoveryModulePpiGuid,
                               0,
                               &RecoveryModuleDescriptor,
                               &RecoveryModulePpi
                               );
    if (EFI_ERROR(Status)) {
      Status = (*PeiServices)->InstallPpi (PeiServices, &mRecoveryPpiList);
    } else {
      Status = (*PeiServices)->ReInstallPpi (
                                 PeiServices,
                                 RecoveryModuleDescriptor,
                                 &mRecoveryPpiList
                                 );
    }

    ASSERT (Status == EFI_SUCCESS);
  }

  return Status;
}


/*++
  This file contains an 'Intel Pre-EFI Module' and is licensed  
  for Intel CPUs and Chipsets under the terms of your license   
  agreement with Intel or your vendor.  This file may be        
  modified by the user, subject to additional terms of the      
  license agreement                                             
--*/
/** @file
  Locate the entry point for the PEI Core

  Copyright (c) 2008 - 2011, Intel Corporation. All rights reserved.<BR>
  This software and associated documentation (if any) is furnished
  under a license and may only be used or copied in accordance
  with the terms of the license. Except as permitted by such
  license, no part of this software or documentation may be
  reproduced, stored in a retrieval system, or transmitted in any
  form or by any means without the express written consent of
  Intel Corporation.

**/

#include <PiPei.h>
#include <Library/BaseLib.h>
#include <Library/PeCoffGetEntryPointLib.h>

#include "SecMain.h"

/**
  Find core image base.
  
  @param   BootFirmwareVolumePtr    Point to the boot firmware volume.
  @param   SecCoreImageBase         The base address of the SEC core image.
  @param   PeiCoreImageBase         The base address of the PEI core image.

**/
EFI_STATUS
EFIAPI
FindImageBase (
  IN  EFI_FIRMWARE_VOLUME_HEADER       *BootFirmwareVolumePtr,
  OUT EFI_PHYSICAL_ADDRESS             *PeiCoreImageBase
  )
{
  EFI_PHYSICAL_ADDRESS        CurrentAddress;
  EFI_PHYSICAL_ADDRESS        EndOfFirmwareVolume;
  EFI_FFS_FILE_HEADER         *File;
  UINT32                      Size;
  EFI_PHYSICAL_ADDRESS        EndOfFile;
  EFI_COMMON_SECTION_HEADER   *Section;
  EFI_PHYSICAL_ADDRESS        EndOfSection;

  *PeiCoreImageBase = 0;

  CurrentAddress = (EFI_PHYSICAL_ADDRESS)(UINTN) BootFirmwareVolumePtr;
  EndOfFirmwareVolume = CurrentAddress + BootFirmwareVolumePtr->FvLength;

  //
  // Loop through the FFS files in the Boot Firmware Volume
  //
  for (EndOfFile = CurrentAddress + BootFirmwareVolumePtr->HeaderLength; ; ) {

    CurrentAddress = (EndOfFile + 7) & 0xfffffffffffffff8ULL;
    if (CurrentAddress > EndOfFirmwareVolume) {
      return EFI_NOT_FOUND;
    }

    File = (EFI_FFS_FILE_HEADER*)(UINTN) CurrentAddress;
    if (IS_FFS_FILE2 (File)) {
      Size = FFS_FILE2_SIZE (File);
      if (Size <= 0x00FFFFFF) {
        return EFI_NOT_FOUND;
      }
    } else {
      Size = FFS_FILE_SIZE (File);
      if (Size < sizeof (EFI_FFS_FILE_HEADER)) {
        return EFI_NOT_FOUND;
      }
    }

    EndOfFile = CurrentAddress + Size;
    if (EndOfFile > EndOfFirmwareVolume) {
      return EFI_NOT_FOUND;
    }

    //
    // Look for SEC Core / PEI Core files
    //
    if (File->Type != EFI_FV_FILETYPE_PEI_CORE) {
      continue;
    }

    //
    // Loop through the FFS file sections within the FFS file
    //
    if (IS_FFS_FILE2 (File)) {
      EndOfSection = (EFI_PHYSICAL_ADDRESS) (UINTN) ((UINT8 *) File + sizeof (EFI_FFS_FILE_HEADER2));
    } else {
      EndOfSection = (EFI_PHYSICAL_ADDRESS) (UINTN) ((UINT8 *) File + sizeof (EFI_FFS_FILE_HEADER));
    }
    for (;;) {
      CurrentAddress = (EndOfSection + 3) & 0xfffffffffffffffcULL;
      Section = (EFI_COMMON_SECTION_HEADER*)(UINTN) CurrentAddress;

      if (IS_SECTION2 (Section)) {
        Size = SECTION2_SIZE (Section);
        if (Size <= 0x00FFFFFF) {
          return EFI_NOT_FOUND;
        }
      } else {
        Size = SECTION_SIZE (Section);
        if (Size < sizeof (EFI_COMMON_SECTION_HEADER)) {
          return EFI_NOT_FOUND;
        }
      }

      EndOfSection = CurrentAddress + Size;
      if (EndOfSection > EndOfFile) {
        return EFI_NOT_FOUND;
      }

      //
      // Look for executable sections
      //
      if (Section->Type == EFI_SECTION_PE32 || Section->Type == EFI_SECTION_TE) {
        if (IS_SECTION2 (Section)) {
          *PeiCoreImageBase = (PHYSICAL_ADDRESS) (UINTN) ((UINT8 *) Section + sizeof (EFI_COMMON_SECTION_HEADER2));
        } else {
          *PeiCoreImageBase = (PHYSICAL_ADDRESS) (UINTN) ((UINT8 *) Section + sizeof (EFI_COMMON_SECTION_HEADER));
        }
        break;
      }
    }

    if (*PeiCoreImageBase) {
      return EFI_SUCCESS;
    }
  }
}

/**
  Find and return Pei Core entry point.

  It also find SEC and PEI Core file debug inforamtion. It will report them if
  remote debug is enabled.
  
  @param   BootFirmwareVolumePtr    Point to the boot firmware volume.
  @param   PeiCoreEntryPoint        The entry point of the PEI core.

**/
VOID
EFIAPI
FindAndReportEntryPoints (
  IN  EFI_FIRMWARE_VOLUME_HEADER       *BootFirmwareVolumePtr,
  OUT EFI_PEI_CORE_ENTRY_POINT         *PeiCoreEntryPoint
  )
{
  EFI_STATUS                       Status;
  EFI_PHYSICAL_ADDRESS             PeiCoreImageBase;


  Status = FindImageBase (BootFirmwareVolumePtr, &PeiCoreImageBase);
  ASSERT_EFI_ERROR (Status);

  Status = PeCoffLoaderGetEntryPoint ((VOID *) (UINTN) PeiCoreImageBase, (VOID**) PeiCoreEntryPoint);
  if (EFI_ERROR (Status)) {
    *PeiCoreEntryPoint = 0;
  }

  return;
}


/** @file

Copyright (c) 2006 - 2011, Byosoft Corporation.<BR>
All rights reserved.This software and associated documentation (if any)
is furnished under a license and may only be used or copied in
accordance with the terms of the license. Except as permitted by such
license, no part of this software or documentation may be reproduced,
stored in a retrieval system, or transmitted in any form or by any
means without the express written consent of Byosoft Corporation.

File Name:
  BiosIdLib.c

Abstract:
  Boot service DXE BIOS ID library implementation.

Revision History:

Bug 2263:   Needs to move R8SnbClientPkg\Platform\LegacyBiosPlatform
            to SnbClientX64Pkg
TIME:       2011-06-21
$AUTHOR:    Liu Chunling
$REVIEWERS:
$SCOPE:     Sugar Bay Customer Refernce Board.
$TECHNICAL: 
  1. Change the name of LegacyBiosPlatform.inf to 
     LegacyBiosPlatformDxe.inf and Revise the coding style 
     following latest standard.
  2. Use EDKII libraries instead of EDK libraries.
  3. Add gWdtProtocolGuid to PlatformPkg.dec.
  4. Move the library BiosIdLib to ByoModulePkg.
$END--------------------------------------------------------------------

**/
/*++
 This file contains an 'Intel Peripheral Driver' and is        
 licensed for Intel CPUs and chipsets under the terms of your  
 license agreement with Intel or your vendor.  This file may   
 be modified by the user, subject to additional terms of the   
 license agreement                                             
--*/
/*++

Copyright (c)  2010 Intel Corporation. All rights reserved
This software and associated documentation (if any) is furnished
under a license and may only be used or copied in accordance
with the terms of the license. Except as permitted by such
license, no part of this software or documentation may be
reproduced, stored in a retrieval system, or transmitted in any
form or by any means without the express written consent of
Intel Corporation.

Module Name:

  BiosIdLib.c
    
Abstract:

  Boot service DXE BIOS ID library implementation.

  These functions in this file can be called during DXE and cannot be called during runtime 
  or in SMM which should use a RT or SMM library.

--*/

#include <Library/BiosIdLib.h>
#include <Guid/BiosId.h>

#include <Library/BaseLib.h>
#include <Library/DxeServicesLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>

EFI_STATUS
GetBiosId (
  OUT BIOS_ID_IMAGE     *BiosIdImage
  )
/*++
Description:

  This function returns BIOS ID by searching HOB or FV.

Arguments:

  BiosIdImage - The BIOS ID got from HOB or FV
  
Returns:

  EFI_SUCCESS - All parameters were valid and BIOS ID has been got.

  EFI_NOT_FOUND - BiosId image is not found, and no parameter will be modified.

  EFI_INVALID_PARAMETER - The parameter is NULL
     
--*/
{
  EFI_STATUS    Status;
  VOID          *Address = NULL;
  UINTN         Size = 0;


  if (BiosIdImage == NULL) {
    return EFI_INVALID_PARAMETER;
  }


    DEBUG ((DEBUG_INFO, "Get BIOS ID from FV\n"));

    Status = GetSectionFromFv (
               &gEfiBiosIdGuid,
               EFI_SECTION_RAW,
               0,
               &Address,
               &Size
               );

    if (Status == EFI_SUCCESS) {
      //
      // BiosId image is present in FV
      //
      if (Address != NULL) {
        Size = sizeof (BIOS_ID_IMAGE);
        CopyMem ((VOID *) BiosIdImage, Address, Size);
        //
        // GetImage () allocated buffer for Address, now clear it
        //
        FreePool (Address);

        DEBUG ((DEBUG_INFO, "Get BIOS ID from FV successfully\n"));
        DEBUG ((DEBUG_INFO, "BIOS ID: %s\n", (CHAR16 *) (&(BiosIdImage->BiosIdString))));

        return EFI_SUCCESS;
      }
    }

  return EFI_NOT_FOUND;
}

EFI_STATUS
GetBiosVersionDateTime (
  OUT CHAR16    *BiosVersion, 
  OUT CHAR16    *BiosReleaseDate,
  OUT CHAR16    *BiosReleaseTime OPTIONAL
  )
/*++
Description:

  This function returns the Version & Release date and time by getting and converting
  BIOS ID.

Arguments:

  BiosVersion - The Bios Version out of the conversion

  BiosReleaseDate - The Bios Release Date out of the conversion

  BiosReleaseTime - The Bios Release Time out of the conversion
  
Returns:

  EFI_SUCCESS - All parameters were valid and Version & Release Date have been set.

  EFI_NOT_FOUND - BiosId image is not found, and no parameter will be modified.

  EFI_INVALID_PARAMETER - One of the parameters is NULL
     
--*/
{
  EFI_STATUS    Status;
  BIOS_ID_IMAGE BiosIdImage;
  
  if ((BiosVersion == NULL) || (BiosReleaseDate == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = GetBiosId (&BiosIdImage);
  if (EFI_ERROR (Status)) {
    return EFI_NOT_FOUND;
  }
  
  //
  // Fill the BiosVersion data from the BIOS ID.
  //
  StrCpy (BiosVersion, (CHAR16 *) (&(BiosIdImage.BiosIdString)));
  
  //
  // Fill the build timestamp data from the BIOS ID in the "MM/DD/YY" format.
  //
  
  BiosReleaseDate[0] = BiosIdImage.BiosIdString.TimeStamp[2];
  BiosReleaseDate[1] = BiosIdImage.BiosIdString.TimeStamp[3];
  BiosReleaseDate[2] = (CHAR16) ((UINT8) ('/'));

  BiosReleaseDate[3] = BiosIdImage.BiosIdString.TimeStamp[4];
  BiosReleaseDate[4] = BiosIdImage.BiosIdString.TimeStamp[5];
  BiosReleaseDate[5] = (CHAR16) ((UINT8) ('/'));

  //
  // Add 20 for SMBIOS table
  // Current Linux kernel will misjudge 09 as year 0, so using 2009 for SMBIOS table
  //
  BiosReleaseDate[6] = '2';
  BiosReleaseDate[7] = '0';
  BiosReleaseDate[8] = BiosIdImage.BiosIdString.TimeStamp[0];
  BiosReleaseDate[9] = BiosIdImage.BiosIdString.TimeStamp[1];

  BiosReleaseDate[10] = (CHAR16) ((UINT8) ('\0'));

  if (BiosReleaseTime != NULL) {

    //
    // Fill the build timestamp time from the BIOS ID in the "HH:MM" format.
    //
  
    BiosReleaseTime[0] = BiosIdImage.BiosIdString.TimeStamp[6];
    BiosReleaseTime[1] = BiosIdImage.BiosIdString.TimeStamp[7];
    BiosReleaseTime[2] = (CHAR16) ((UINT8) (':'));

    BiosReleaseTime[3] = BiosIdImage.BiosIdString.TimeStamp[8];
    BiosReleaseTime[4] = BiosIdImage.BiosIdString.TimeStamp[9];

    BiosReleaseTime[5] = (CHAR16) ((UINT8) ('\0'));
  }
  
  return  EFI_SUCCESS;
}

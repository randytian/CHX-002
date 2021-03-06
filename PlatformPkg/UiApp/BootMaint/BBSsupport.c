/*++

Copyright (c) 2006-2012, Intel Corporation
All rights reserved. This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

Module Description:

  This function deal with the legacy boot option, it create, delete
  and manage the legacy boot option, all legacy boot option is getting from
  the legacy BBS table.


**/

#include "BBSsupport.h"


/**
  Re-order the Boot Option according to the DevOrder.

  The routine re-orders the Boot Option in BootOption array according to
  the order specified by DevOrder.

  @param DevOrder           Pointer to buffer containing the BBS Index,
                            high 8-bit value 0xFF indicating a disabled boot option
  @param DevOrderCount      Count of the BBS Index
  @param EnBootOption       Callee allocated buffer containing the enabled Boot Option Numbers
  @param EnBootOptionCount  Count of the enabled Boot Option Numbers
  @param DisBootOption      Callee allocated buffer containing the disabled Boot Option Numbers
  @param DisBootOptionCount Count of the disabled Boot Option Numbers
**/
VOID
OrderLegacyBootOption4SameType (
  UINT16                   *DevOrder,
  UINTN                    DevOrderCount,
  UINT16                   **EnBootOption,
  UINTN                    *EnBootOptionCount,
  UINT16                   **DisBootOption,
  UINTN                    *DisBootOptionCount
  )
{
  EFI_STATUS               Status;
  UINT16                   *NewBootOption;
  UINT16                   *BootOrder;
  UINTN                    BootOrderSize;
  UINTN                    Index;
  UINTN                    StartPosition;
  
  EFI_BOOT_MANAGER_LOAD_OPTION    BootOption;
  
  CHAR16                           OptionName[sizeof ("Boot####")];
  UINT16                   *BbsIndexArray;
  UINT16                   *DeviceTypeArray;

  BootOrder = EfiBootManagerGetVariableAndSize (
                L"BootOrder",
                &gEfiGlobalVariableGuid,
                &BootOrderSize
                );
  ASSERT (BootOrder != NULL);

  BbsIndexArray       = AllocatePool (BootOrderSize);
  DeviceTypeArray     = AllocatePool (BootOrderSize);
  *EnBootOption       = AllocatePool (BootOrderSize);
  *DisBootOption      = AllocatePool (BootOrderSize);
  *DisBootOptionCount = 0;
  *EnBootOptionCount  = 0;
  Index               = 0;

  ASSERT (*EnBootOption != NULL);
  ASSERT (*DisBootOption != NULL);

  for (Index = 0; Index < BootOrderSize / sizeof (UINT16); Index++) {
  
    UnicodeSPrint (OptionName, sizeof (OptionName), L"Boot%04x", BootOrder[Index]);
    Status = EfiBootManagerVariableToLoadOption (OptionName, &BootOption);
    ASSERT_EFI_ERROR (Status);
    
    if ((DevicePathType (BootOption.FilePath) == BBS_DEVICE_PATH) &&
        (DevicePathSubType (BootOption.FilePath) == BBS_BBS_DP)) {
      //
      // Legacy Boot Option
      //
      ASSERT (BootOption.OptionalDataSize == sizeof (LEGACY_BOOT_OPTION_BBS_DATA));

      DeviceTypeArray[Index] = ((BBS_BBS_DEVICE_PATH *) BootOption.FilePath)->DeviceType;
      BbsIndexArray  [Index] = ((LEGACY_BOOT_OPTION_BBS_DATA *) BootOption.OptionalData)->BbsIndex;
    } else {
      DeviceTypeArray[Index] = BBS_TYPE_UNKNOWN;
      BbsIndexArray  [Index] = 0xFFFF;
    }
    EfiBootManagerFreeLoadOption (&BootOption);
  }

  //
  // Record the corresponding Boot Option Numbers according to the DevOrder
  // Record the EnBootOption and DisBootOption according to the DevOrder
  //
  StartPosition = BootOrderSize / sizeof (UINT16);
  NewBootOption = AllocatePool (DevOrderCount * sizeof (UINT16));
  while (DevOrderCount-- != 0) {
    for (Index = 0; Index < BootOrderSize / sizeof (UINT16); Index++) {
      if (BbsIndexArray[Index] == (DevOrder[DevOrderCount] & 0xFF)) {
        StartPosition = MIN (StartPosition, Index);
        NewBootOption[DevOrderCount] = BootOrder[Index];
        
        if ((DevOrder[DevOrderCount] & 0xFF00) == 0xFF00) {
          (*DisBootOption)[*DisBootOptionCount] = BootOrder[Index];
          (*DisBootOptionCount)++;
        } else {
          (*EnBootOption)[*EnBootOptionCount] = BootOrder[Index];
          (*EnBootOptionCount)++;
        }
        break;
      }
    }
  }

  //
  // Overwrite the old BootOption
  //
  CopyMem (&BootOrder[StartPosition], NewBootOption, (*DisBootOptionCount + *EnBootOptionCount) * sizeof (UINT16));
  Status = gRT->SetVariable (
                  L"BootOrder",
                  &gEfiGlobalVariableGuid,
                  VAR_FLAG,
                  BootOrderSize,
                  BootOrder
                  );
  ASSERT_EFI_ERROR (Status);

  FreePool (NewBootOption);
  FreePool (DeviceTypeArray);
  FreePool (BbsIndexArray);
}

/**
  Group the legacy boot options in the BootOption.

  The routine assumes the boot options in the beginning that covers all the device 
  types are ordered properly and re-position the following boot options just after
  the corresponding boot options with the same device type.
  For example:
  1. Input  = [Harddisk1 CdRom2 Efi1 Harddisk0 CdRom0 CdRom1 Harddisk2 Efi0]
     Assuming [Harddisk1 CdRom2 Efi1] is ordered properly
     Output = [Harddisk1 Harddisk0 Harddisk2 CdRom2 CdRom0 CdRom1 Efi1 Efi0]

  2. Input  = [Efi1 Efi0 CdRom1 Harddisk0 Harddisk1 Harddisk2 CdRom0 CdRom2]
     Assuming [Efi1 Efi0 CdRom1 Harddisk0] is ordered properly
     Output = [Efi1 Efi0 CdRom1 CdRom0 CdRom2 Harddisk0 Harddisk1 Harddisk2]

**/
VOID
GroupMultipleLegacyBootOption4SameType (
  VOID
  )
{
  EFI_STATUS                   Status;
  UINTN                        Index;
  UINTN                        DeviceIndex;
  UINTN                        DeviceTypeIndex[7];
  UINTN                        *NextIndex;
  UINT16                       OptionNumber;
  UINT16                       *BootOrder = NULL;
  UINTN                        BootOrderSize;
  CHAR16                       OptionName[sizeof ("Boot####")];
  EFI_BOOT_MANAGER_LOAD_OPTION BootOption;

  SetMem (DeviceTypeIndex, sizeof (DeviceTypeIndex), 0xff);

  BootOrder = EfiBootManagerGetVariableAndSize (
                L"BootOrder",
                &gEfiGlobalVariableGuid,
                &BootOrderSize
                );
  if(BootOrderSize == 0){
    return;
  }  


  for (Index = 0; Index < BootOrderSize / sizeof (UINT16); Index++) {
    UnicodeSPrint (OptionName, sizeof (OptionName), L"Boot%04x", BootOrder[Index]);
    Status = EfiBootManagerVariableToLoadOption (OptionName, &BootOption);
    ASSERT_EFI_ERROR (Status);

    if ((DevicePathType (BootOption.FilePath) == BBS_DEVICE_PATH) &&
        (DevicePathSubType (BootOption.FilePath) == BBS_BBS_DP)) {
      //
      // Legacy Boot Option
      //
      ASSERT ((((BBS_BBS_DEVICE_PATH *) BootOption.FilePath)->DeviceType & 0xF) < sizeof (DeviceTypeIndex) / sizeof (DeviceTypeIndex[0]));
      NextIndex = &DeviceTypeIndex[((BBS_BBS_DEVICE_PATH *) BootOption.FilePath)->DeviceType & 0xF];

      if (*NextIndex == (UINTN) -1) {
        //
        // *NextIndex is the Index in BootOrder to put the next Option Number for the same type
        //
        *NextIndex = Index + 1;
      } else {
        //
        // insert the current boot option before *NextIndex, causing [*Next .. Index] shift right one position
        //
        OptionNumber = BootOrder[Index];
        CopyMem (&BootOrder[*NextIndex + 1], &BootOrder[*NextIndex], (Index - *NextIndex) * sizeof (UINT16));
        BootOrder[*NextIndex] = OptionNumber;

        //
        // Update the DeviceTypeIndex array to reflect the right shift operation
        //
        for (DeviceIndex = 0; DeviceIndex < sizeof (DeviceTypeIndex) / sizeof (DeviceTypeIndex[0]); DeviceIndex++) {
          if (DeviceTypeIndex[DeviceIndex] != (UINTN) -1 && DeviceTypeIndex[DeviceIndex] >= *NextIndex) {
            DeviceTypeIndex[DeviceIndex]++;
          }
        }
      }
    }
    EfiBootManagerFreeLoadOption (&BootOption);
  }

  gRT->SetVariable (
         L"BootOrder",
         &gEfiGlobalVariableGuid,
         VAR_FLAG,
         BootOrderSize,
         BootOrder
         );
  if(BootOrder!=NULL){
    FreePool (BootOrder);
  }  
}

//-----------------------------------------------------------------------------------------------------

/**
  Re-order the Boot Option according to the DevOrder.

  The routine re-orders the Boot Option in BootOption array according to
  the order specified by DevOrder.

  @param DevOrder           Pointer to buffer containing the BBS Index,
                            high 8-bit value 0xFF indicating a disabled boot option
  @param DevOrderCount      Count of the BBS Index
  @param EnBootOption       Callee allocated buffer containing the enabled Boot Option Numbers
  @param EnBootOptionCount  Count of the enabled Boot Option Numbers
  @param DisBootOption      Callee allocated buffer containing the disabled Boot Option Numbers
  @param DisBootOptionCount Count of the disabled Boot Option Numbers
**/
VOID
ByoOrderLegacyBootOption4SameType (
  UINT16                   *DevOrder,
  UINTN                    DevOrderCount,
  UINT16                   **EnBootOption,
  UINTN                    *EnBootOptionCount,
  UINT16                   **DisBootOption,
  UINTN                    *DisBootOptionCount
  )
{
  EFI_STATUS               Status;
  UINT16                   *NewBootOption;
  UINT16                   *BootOrder;
  UINTN                    BootOrderSize;
  UINTN                    Index;
  UINTN                    StartPosition;
  
  EFI_BOOT_MANAGER_LOAD_OPTION    BootOption;
  
  CHAR16                           OptionName[sizeof ("BootTemp####")];
  UINT16                   *BbsIndexArray;
  UINT16                   *DeviceTypeArray;

  BootOrder = EfiBootManagerGetVariableAndSize (
                L"BootOrderTemp",
                &gByoGlobalVariableGuid,
                &BootOrderSize
                );
  ASSERT (BootOrder != NULL);

  BbsIndexArray       = AllocatePool (BootOrderSize);
  DeviceTypeArray     = AllocatePool (BootOrderSize);
  *EnBootOption       = AllocatePool (BootOrderSize);
  *DisBootOption      = AllocatePool (BootOrderSize);
  *DisBootOptionCount = 0;
  *EnBootOptionCount  = 0;
  Index               = 0;

  ASSERT (*EnBootOption != NULL);
  ASSERT (*DisBootOption != NULL);

  for (Index = 0; Index < BootOrderSize / sizeof (UINT16); Index++) {
  
    UnicodeSPrint (OptionName, sizeof (OptionName), L"BootTemp%04x", BootOrder[Index]);
    Status = ByoEfiBootManagerVariableToLoadOption (OptionName, &BootOption);
    ASSERT_EFI_ERROR (Status);
    
    if ((DevicePathType (BootOption.FilePath) == BBS_DEVICE_PATH) &&
        (DevicePathSubType (BootOption.FilePath) == BBS_BBS_DP)) {
      //
      // Legacy Boot Option
      //
      ASSERT (BootOption.OptionalDataSize == sizeof (LEGACY_BOOT_OPTION_BBS_DATA));

      DeviceTypeArray[Index] = ((BBS_BBS_DEVICE_PATH *) BootOption.FilePath)->DeviceType;
      BbsIndexArray  [Index] = ((LEGACY_BOOT_OPTION_BBS_DATA *) BootOption.OptionalData)->BbsIndex;
    } else {
      DeviceTypeArray[Index] = BBS_TYPE_UNKNOWN;
      BbsIndexArray  [Index] = 0xFFFF;
    }
    EfiBootManagerFreeLoadOption (&BootOption);
  }

  //
  // Record the corresponding Boot Option Numbers according to the DevOrder
  // Record the EnBootOption and DisBootOption according to the DevOrder
  //
  StartPosition = BootOrderSize / sizeof (UINT16);
  NewBootOption = AllocatePool (DevOrderCount * sizeof (UINT16));
  while (DevOrderCount-- != 0) {
    for (Index = 0; Index < BootOrderSize / sizeof (UINT16); Index++) {
      if (BbsIndexArray[Index] == (DevOrder[DevOrderCount] & 0xFF)) {
        StartPosition = MIN (StartPosition, Index);
        NewBootOption[DevOrderCount] = BootOrder[Index];
        
        if ((DevOrder[DevOrderCount] & 0xFF00) == 0xFF00) {
          (*DisBootOption)[*DisBootOptionCount] = BootOrder[Index];
          (*DisBootOptionCount)++;
        } else {
          (*EnBootOption)[*EnBootOptionCount] = BootOrder[Index];
          (*EnBootOptionCount)++;
        }
        break;
      }
    }
  }

  //
  // Overwrite the old BootOption
  //
  CopyMem (&BootOrder[StartPosition], NewBootOption, (*DisBootOptionCount + *EnBootOptionCount) * sizeof (UINT16));
  Status = gRT->SetVariable (
                  L"BootOrderTemp",
                  &gByoGlobalVariableGuid,
                  TEMP_VAR_FLAG,
                  BootOrderSize,
                  BootOrder
                  );
  ASSERT_EFI_ERROR (Status);

  FreePool (NewBootOption);
  FreePool (DeviceTypeArray);
  FreePool (BbsIndexArray);
}

/**
  Group the legacy boot options in the BootOption.

  The routine assumes the boot options in the beginning that covers all the device 
  types are ordered properly and re-position the following boot options just after
  the corresponding boot options with the same device type.
  For example:
  1. Input  = [Harddisk1 CdRom2 Efi1 Harddisk0 CdRom0 CdRom1 Harddisk2 Efi0]
     Assuming [Harddisk1 CdRom2 Efi1] is ordered properly
     Output = [Harddisk1 Harddisk0 Harddisk2 CdRom2 CdRom0 CdRom1 Efi1 Efi0]

  2. Input  = [Efi1 Efi0 CdRom1 Harddisk0 Harddisk1 Harddisk2 CdRom0 CdRom2]
     Assuming [Efi1 Efi0 CdRom1 Harddisk0] is ordered properly
     Output = [Efi1 Efi0 CdRom1 CdRom0 CdRom2 Harddisk0 Harddisk1 Harddisk2]

**/
VOID
ByoGroupMultipleLegacyBootOption4SameType (
  VOID
  )
{
  EFI_STATUS                   Status;
  UINTN                        Index;
  UINTN                        DeviceIndex;
  UINTN                        DeviceTypeIndex[7];
  UINTN                        *NextIndex;
  UINT16                       OptionNumber;
  UINT16                       *BootOrder;
  UINTN                        BootOrderSize;
  CHAR16                       OptionName[sizeof ("BootTemp####")];
  EFI_BOOT_MANAGER_LOAD_OPTION BootOption;

  SetMem (DeviceTypeIndex, sizeof (DeviceTypeIndex), 0xff);

  BootOrder = EfiBootManagerGetVariableAndSize (
                L"BootOrderTemp",
                &gByoGlobalVariableGuid,
                &BootOrderSize
                );

  for (Index = 0; Index < BootOrderSize / sizeof (UINT16); Index++) {
    UnicodeSPrint (OptionName, sizeof (OptionName), L"BootTemp%04x", BootOrder[Index]);
    Status = ByoEfiBootManagerVariableToLoadOption (OptionName, &BootOption);
    ASSERT_EFI_ERROR (Status);

    if ((DevicePathType (BootOption.FilePath) == BBS_DEVICE_PATH) &&
        (DevicePathSubType (BootOption.FilePath) == BBS_BBS_DP)) {
      //
      // Legacy Boot Option
      //
      ASSERT ((((BBS_BBS_DEVICE_PATH *) BootOption.FilePath)->DeviceType & 0xF) < sizeof (DeviceTypeIndex) / sizeof (DeviceTypeIndex[0]));
      NextIndex = &DeviceTypeIndex[((BBS_BBS_DEVICE_PATH *) BootOption.FilePath)->DeviceType & 0xF];

      if (*NextIndex == (UINTN) -1) {
        //
        // *NextIndex is the Index in BootOrder to put the next Option Number for the same type
        //
        *NextIndex = Index + 1;
      } else {
        //
        // insert the current boot option before *NextIndex, causing [*Next .. Index] shift right one position
        //
        OptionNumber = BootOrder[Index];
        CopyMem (&BootOrder[*NextIndex + 1], &BootOrder[*NextIndex], (Index - *NextIndex) * sizeof (UINT16));
        BootOrder[*NextIndex] = OptionNumber;

        //
        // Update the DeviceTypeIndex array to reflect the right shift operation
        //
        for (DeviceIndex = 0; DeviceIndex < sizeof (DeviceTypeIndex) / sizeof (DeviceTypeIndex[0]); DeviceIndex++) {
          if (DeviceTypeIndex[DeviceIndex] != (UINTN) -1 && DeviceTypeIndex[DeviceIndex] >= *NextIndex) {
            DeviceTypeIndex[DeviceIndex]++;
          }
        }
      }
    }
    EfiBootManagerFreeLoadOption (&BootOption);
  }

  gRT->SetVariable (
         L"BootOrderTemp",
         &gByoGlobalVariableGuid,
         VAR_FLAG,
         BootOrderSize,
         BootOrder
         );
  FreePool (BootOrder);
}

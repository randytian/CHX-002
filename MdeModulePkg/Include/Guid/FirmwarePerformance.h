/** @file
  ACPI Firmware Performance Data Table (FPDT) implementation specific definitions.

  Copyright (c) 2011, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef _FIRMWARE_PERFORMANCE_GUID_H_
#define _FIRMWARE_PERFORMANCE_GUID_H_

#include <IndustryStandard/Acpi50.h>
#include <PiPei.h>
#include <Ppi/SecPerformance.h>

///
/// This GUID is used for FPDT implementation specific EFI Variable, LockBox and Hob.
///
/// EFI Variable:
///   GUID - gEfiFirmwarePerformanceGuid
///   Name - EFI_FIRMWARE_PERFORMANCE_VARIABLE_NAME
///   Data - FIRMWARE_PERFORMANCE_VARIABLE
///
/// LockBox:
///   GUID - gEfiFirmwarePerformanceGuid
///   Data - EFI_ACPI_BASIC_S3_SUSPEND_PERFORMANCE_RECORD
///
/// Hob:
///   GUID - gEfiFirmwarePerformanceGuid
///   Data - FIRMWARE_SEC_PERFORMANCE (defined in <Ppi/SecPerformance.h>)
///
#define EFI_FIRMWARE_PERFORMANCE_GUID \
  { \
    0xc095791a, 0x3001, 0x47b2, {0x80, 0xc9, 0xea, 0xc7, 0x31, 0x9f, 0x2f, 0xa4 } \
  }

#define EFI_FIRMWARE_PERFORMANCE_VARIABLE_NAME  L"FirmwarePerformance"

#pragma pack(1)

///
/// Firmware Performance Data Table.
/// This structure will be installed into ACPI table as FPDT in normal boot path.
///
typedef struct {
  EFI_ACPI_DESCRIPTION_HEADER                             Header;            ///< Common ACPI description table header.
  EFI_ACPI_5_0_FPDT_BOOT_PERFORMANCE_TABLE_POINTER_RECORD BootPointerRecord; ///< Basic Boot Performance Table Pointer record.
  EFI_ACPI_5_0_FPDT_S3_PERFORMANCE_TABLE_POINTER_RECORD   S3PointerRecord;   ///< S3 Performance Table Pointer record.
} FIRMWARE_PERFORMANCE_TABLE;

///
/// S3 Performance Data Table.
/// This structure contains S3 performance records which will be updated in S3
/// suspend and S3 resume boot path.
///
typedef struct {
  EFI_ACPI_5_0_FPDT_PERFORMANCE_TABLE_HEADER  Header;    ///< Common ACPI table header.
  EFI_ACPI_5_0_FPDT_S3_RESUME_RECORD          S3Resume;  ///< Basic S3 Resume performance record.
  EFI_ACPI_5_0_FPDT_S3_SUSPEND_RECORD         S3Suspend; ///< Basic S3 Suspend performance record.
} S3_PERFORMANCE_TABLE;

///
/// Basic Boot Performance Data Table.
/// This structure contains BasicBoot performance record.
///
typedef struct {
  EFI_ACPI_5_0_FPDT_PERFORMANCE_TABLE_HEADER   Header;     ///< Common ACPI table header.
  EFI_ACPI_5_0_FPDT_FIRMWARE_BASIC_BOOT_RECORD BasicBoot;  ///< Basic Boot Resume performance record.
} BOOT_PERFORMANCE_TABLE;

///
/// Performance data pointed by Performance Pointer Record.
///
typedef struct {
  BOOT_PERFORMANCE_TABLE         BootPerformance; ///< Basic Boot Performance.
  S3_PERFORMANCE_TABLE           S3Performance;   ///< S3 performance.
} FIRMWARE_PERFORMANCE_RUNTIME_DATA;

///
/// Variable defined for FPDT implementation.
/// This Variable is produced by FPDT DXE module and consumed by FPDT PEIM.
///
typedef struct {
  EFI_PHYSICAL_ADDRESS  BootPerformanceTablePointer; ///< Pointer to Boot Performance Table.
  EFI_PHYSICAL_ADDRESS  S3PerformanceTablePointer;   ///< Pointer to S3 Performance Table.
} FIRMWARE_PERFORMANCE_VARIABLE;

#pragma pack()

extern EFI_GUID gEfiFirmwarePerformanceGuid;

#endif

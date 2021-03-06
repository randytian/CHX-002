/** @file
  Implement TPM2 help.

Copyright (c) 2013 - 2016, Intel Corporation. All rights reserved. <BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <IndustryStandard/UefiTcgPlatform.h>
#include <Library/Tpm2CommandLib.h>
#include <Library/Tpm2DeviceLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>

typedef struct {
  TPMI_ALG_HASH              HashAlgo;
  UINT16                     HashSize;
} INTERNAL_HASH_INFO;

STATIC INTERNAL_HASH_INFO mHashInfo[] = {
  {TPM_ALG_SHA1,          SHA1_DIGEST_SIZE},
  {TPM_ALG_SHA256,        SHA256_DIGEST_SIZE},
  {TPM_ALG_SM3_256,       SM3_256_DIGEST_SIZE},
  {TPM_ALG_SHA384,        SHA384_DIGEST_SIZE},
  {TPM_ALG_SHA512,        SHA512_DIGEST_SIZE},
};

/**
  Return size of digest.

  @param[in] HashAlgo  Hash algorithm

  @return size of digest
**/
UINT16
EFIAPI
GetHashSizeFromAlgo (
  IN TPMI_ALG_HASH    HashAlgo
  )
{
  UINTN  Index;

  for (Index = 0; Index < sizeof(mHashInfo)/sizeof(mHashInfo[0]); Index++) {
    if (mHashInfo[Index].HashAlgo == HashAlgo) {
      return mHashInfo[Index].HashSize;
    }
  }
  return 0;
}

/**
  Copy AuthSessionIn to TPM2 command buffer.

  @param [in]  AuthSessionIn   Input AuthSession data
  @param [out] AuthSessionOut  Output AuthSession data in TPM2 command buffer

  @return AuthSession size
**/
UINT32
EFIAPI
CopyAuthSessionCommand (
  IN      TPMS_AUTH_COMMAND         *AuthSessionIn, OPTIONAL
  OUT     UINT8                     *AuthSessionOut
  )
{
  UINT8  *Buffer;

  Buffer = (UINT8 *)AuthSessionOut;
  
  //
  // Add in Auth session
  //
  if (AuthSessionIn != NULL) {
    //  sessionHandle
    WriteUnaligned32 ((UINT32 *)Buffer, SwapBytes32(AuthSessionIn->sessionHandle));
    Buffer += sizeof(UINT32);

    // nonce
    WriteUnaligned16 ((UINT16 *)Buffer, SwapBytes16 (AuthSessionIn->nonce.size));
    Buffer += sizeof(UINT16);

    CopyMem (Buffer, AuthSessionIn->nonce.buffer, AuthSessionIn->nonce.size);
    Buffer += AuthSessionIn->nonce.size;

    // sessionAttributes
    *(UINT8 *)Buffer = *(UINT8 *)&AuthSessionIn->sessionAttributes;
    Buffer++;

    // hmac
    WriteUnaligned16 ((UINT16 *)Buffer, SwapBytes16 (AuthSessionIn->hmac.size));
    Buffer += sizeof(UINT16);

    CopyMem (Buffer, AuthSessionIn->hmac.buffer, AuthSessionIn->hmac.size);
    Buffer += AuthSessionIn->hmac.size;
  } else {
    //  sessionHandle
    WriteUnaligned32 ((UINT32 *)Buffer, SwapBytes32(TPM_RS_PW));
    Buffer += sizeof(UINT32);

    // nonce = nullNonce
    WriteUnaligned16 ((UINT16 *)Buffer, SwapBytes16(0));
    Buffer += sizeof(UINT16);

    // sessionAttributes = 0
    *(UINT8 *)Buffer = 0x00;
    Buffer++;

    // hmac = nullAuth
    WriteUnaligned16 ((UINT16 *)Buffer, SwapBytes16(0));
    Buffer += sizeof(UINT16);
  }

  return (UINT32)(UINTN)(Buffer - (UINT8 *)AuthSessionOut);
}

/**
  Copy AuthSessionIn from TPM2 response buffer.

  @param [in]  AuthSessionIn   Input AuthSession data in TPM2 response buffer
  @param [out] AuthSessionOut  Output AuthSession data

  @return AuthSession size
**/
UINT32
EFIAPI
CopyAuthSessionResponse (
  IN      UINT8                      *AuthSessionIn,
  OUT     TPMS_AUTH_RESPONSE         *AuthSessionOut OPTIONAL
  )
{
  UINT8                      *Buffer;
  TPMS_AUTH_RESPONSE         LocalAuthSessionOut;

  if (AuthSessionOut == NULL) {
    AuthSessionOut = &LocalAuthSessionOut;
  }

  Buffer = (UINT8 *)AuthSessionIn;

  // nonce
  AuthSessionOut->nonce.size = SwapBytes16 (ReadUnaligned16 ((UINT16 *)Buffer));
  Buffer += sizeof(UINT16);

  CopyMem (AuthSessionOut->nonce.buffer, Buffer, AuthSessionOut->nonce.size);
  Buffer += AuthSessionOut->nonce.size;

  // sessionAttributes
  *(UINT8 *)&AuthSessionOut->sessionAttributes = *(UINT8 *)Buffer;
  Buffer++;

  // hmac
  AuthSessionOut->hmac.size = SwapBytes16 (ReadUnaligned16 ((UINT16 *)Buffer));
  Buffer += sizeof(UINT16);

  CopyMem (AuthSessionOut->hmac.buffer, Buffer, AuthSessionOut->hmac.size);
  Buffer += AuthSessionOut->hmac.size;

  return (UINT32)(UINTN)(Buffer - (UINT8 *)AuthSessionIn);
}

/**
  Return if hash alg is supported in HashAlgorithmMask.

  @param HashAlg            Hash algorithm to be checked.
  @param HashAlgorithmMask  Bitfield of allowed hash algorithms.

  @retval TRUE  Hash algorithm is supported.
  @retval FALSE Hash algorithm is not supported.
**/
BOOLEAN
IsHashAlgSupportedInHashAlgorithmMask(
  IN TPMI_ALG_HASH  HashAlg,
  IN UINT32         HashAlgorithmMask
  )
{
  switch (HashAlg) {
  case TPM_ALG_SHA1:
    if ((HashAlgorithmMask & HASH_ALG_SHA1) != 0) {
      return TRUE;
    }
    break;
  case TPM_ALG_SHA256:
    if ((HashAlgorithmMask & HASH_ALG_SHA256) != 0) {
      return TRUE;
    }
    break;
  case TPM_ALG_SHA384:
    if ((HashAlgorithmMask & HASH_ALG_SHA384) != 0) {
      return TRUE;
    }
    break;
  case TPM_ALG_SHA512:
    if ((HashAlgorithmMask & HASH_ALG_SHA512) != 0) {
      return TRUE;
    }
    break;
  case TPM_ALG_SM3_256:
    if ((HashAlgorithmMask & HASH_ALG_SM3_256) != 0) {
      return TRUE;
    }
    break;
  }

  return FALSE;
}

/**
  Copy TPML_DIGEST_VALUES into a buffer

  @param[in,out] Buffer             Buffer to hold TPML_DIGEST_VALUES.
  @param[in]     DigestList         TPML_DIGEST_VALUES to be copied.
  @param[in]     HashAlgorithmMask  HASH bits corresponding to the desired digests to copy.

  @return The end of buffer to hold TPML_DIGEST_VALUES.
**/
VOID *
EFIAPI
CopyDigestListToBuffer (
  IN OUT VOID                       *Buffer,
  IN TPML_DIGEST_VALUES             *DigestList,
  IN UINT32                         HashAlgorithmMask
  )
{
  UINTN  Index;
  UINT16 DigestSize;

  CopyMem (Buffer, &DigestList->count, sizeof(DigestList->count));
  Buffer = (UINT8 *)Buffer + sizeof(DigestList->count);
  for (Index = 0; Index < DigestList->count; Index++) {
    if (!IsHashAlgSupportedInHashAlgorithmMask(DigestList->digests[Index].hashAlg, HashAlgorithmMask)) {
      DEBUG ((EFI_D_ERROR, "WARNING: TPM2 Event log has HashAlg unsupported by PCR bank (0x%x)\n", DigestList->digests[Index].hashAlg));
      continue;
    }
    CopyMem (Buffer, &DigestList->digests[Index].hashAlg, sizeof(DigestList->digests[Index].hashAlg));
    Buffer = (UINT8 *)Buffer + sizeof(DigestList->digests[Index].hashAlg);
    DigestSize = GetHashSizeFromAlgo (DigestList->digests[Index].hashAlg);
    CopyMem (Buffer, &DigestList->digests[Index].digest, DigestSize);
    Buffer = (UINT8 *)Buffer + DigestSize;
  }

  return Buffer;
}

/**
  Get TPML_DIGEST_VALUES data size.

  @param[in]     DigestList    TPML_DIGEST_VALUES data.

  @return TPML_DIGEST_VALUES data size.
**/
UINT32
EFIAPI
GetDigestListSize (
  IN TPML_DIGEST_VALUES             *DigestList
  )
{
  UINTN  Index;
  UINT16 DigestSize;
  UINT32 TotalSize;

  TotalSize = sizeof(DigestList->count);
  for (Index = 0; Index < DigestList->count; Index++) {
    DigestSize = GetHashSizeFromAlgo (DigestList->digests[Index].hashAlg);
    TotalSize += sizeof(DigestList->digests[Index].hashAlg) + DigestSize;
  }

  return TotalSize;
}

/**
  This function get digest from digest list.

  @param[in]  HashAlg       Digest algorithm
  @param[in]  DigestList    Digest list
  @param[out] Digest        Digest

  @retval EFI_SUCCESS       Digest is found and returned.
  @retval EFI_NOT_FOUND     Digest is not found.
**/
EFI_STATUS
EFIAPI
GetDigestFromDigestList (
  IN TPMI_ALG_HASH      HashAlg,
  IN TPML_DIGEST_VALUES *DigestList,
  OUT VOID              *Digest
  )
{
  UINTN  Index;
  UINT16 DigestSize;

  DigestSize = GetHashSizeFromAlgo (HashAlg);
  for (Index = 0; Index < DigestList->count; Index++) {
    if (DigestList->digests[Index].hashAlg == HashAlg) {
      CopyMem (
        Digest,
        &DigestList->digests[Index].digest,
        DigestSize
        );
      return EFI_SUCCESS;
    }
  }

  return EFI_NOT_FOUND;
}

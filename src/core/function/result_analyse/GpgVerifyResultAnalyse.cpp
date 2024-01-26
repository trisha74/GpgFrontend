/**
 * Copyright (C) 2021 Saturneric <eric@bktus.com>
 *
 * This file is part of GpgFrontend.
 *
 * GpgFrontend is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * GpgFrontend is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GpgFrontend. If not, see <https://www.gnu.org/licenses/>.
 *
 * The initial version of the source code is inherited from
 * the gpg4usb project, which is under GPL-3.0-or-later.
 *
 * All the source code of GpgFrontend was modified and released by
 * Saturneric <eric@bktus.com> starting on May 12, 2021.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include "GpgVerifyResultAnalyse.h"

#include "core/GpgModel.h"
#include "core/function/gpg/GpgKeyGetter.h"
#include "core/utils/CommonUtils.h"
#include "core/utils/LocalizedUtils.h"

GpgFrontend::GpgVerifyResultAnalyse::GpgVerifyResultAnalyse(
    GpgError error, GpgVerifyResult result)
    : error_(error), result_(result) {}

void GpgFrontend::GpgVerifyResultAnalyse::doAnalyse() {
  auto *result = this->result_.GetRaw();

  stream_ << "# " << tr("Verify Operation") << " ";

  if (gpgme_err_code(error_) == GPG_ERR_NO_ERROR) {
    stream_ << " - " << tr("Success") << " " << Qt::endl;
  } else {
    stream_ << " - " << tr("Failed") << ": " << gpgme_strerror(error_)
            << Qt::endl;
    setStatus(-1);
  }

  if (result != nullptr && result->signatures != nullptr) {
    stream_ << Qt::endl;
    auto *sign = result->signatures;

    stream_ << "-> " << tr("Signed On") << "(" << tr("UTC") << ")"
            << ": " << QDateTime::fromSecsSinceEpoch(sign->timestamp).toString()
            << Qt::endl;

    stream_ << "-> " << tr("Signed On") << "(" << tr("Localized") << ")"
            << ": " << GetFormatedDateByTimestamp(sign->timestamp) << Qt::endl;

    stream_ << Qt::endl << "## " << tr("Signatures List") << ":" << Qt::endl;
    stream_ << Qt::endl;

    bool can_continue = true;

    int count = 1;
    while ((sign != nullptr) && can_continue) {
      stream_ << "### " << tr("Signature [%1]:").arg(count++) << Qt::endl;
      stream_ << "- " << tr("Status") << ": ";
      switch (gpg_err_code(sign->status)) {
        case GPG_ERR_BAD_SIGNATURE:
          stream_ << tr("A Bad Signature.") << Qt::endl;
          print_signer(stream_, GpgSignature(sign));
          stream_ << tr("This Signature is invalid.") << Qt::endl;
          can_continue = false;
          setStatus(-1);
          break;
        case GPG_ERR_NO_ERROR:
          stream_ << tr("A") << " ";
          if ((sign->summary & GPGME_SIGSUM_GREEN) != 0) {
            stream_ << tr("Good") << " ";
          }
          if ((sign->summary & GPGME_SIGSUM_RED) != 0) {
            stream_ << tr("Bad") << " ";
          }
          if ((sign->summary & GPGME_SIGSUM_SIG_EXPIRED) != 0) {
            stream_ << tr("Expired") << " ";
          }
          if ((sign->summary & GPGME_SIGSUM_KEY_MISSING) != 0) {
            stream_ << tr("Missing Key's") << " ";
          }
          if ((sign->summary & GPGME_SIGSUM_KEY_REVOKED) != 0) {
            stream_ << tr("Revoked Key's") << " ";
          }
          if ((sign->summary & GPGME_SIGSUM_KEY_EXPIRED) != 0) {
            stream_ << tr("Expired Key's") << " ";
          }
          if ((sign->summary & GPGME_SIGSUM_CRL_MISSING) != 0) {
            stream_ << tr("Missing CRL's") << " ";
          }

          if ((sign->summary & GPGME_SIGSUM_VALID) != 0) {
            stream_ << tr("Signature Fully Valid.") << Qt::endl;
          } else {
            stream_ << tr("Signature Not Fully Valid.") << Qt::endl;
            stream_ << "- " << tr("Tips") << ": "
                    << tr("Adjust Trust Level to make it Fully Vaild")
                    << Qt::endl;
          }

          if ((sign->status & GPGME_SIGSUM_KEY_MISSING) == 0U) {
            if (!print_signer(stream_, GpgSignature(sign))) setStatus(0);
          } else {
            stream_ << tr("Key is NOT present with ID 0x") << sign->fpr
                    << Qt::endl;
          }

          setStatus(1);

          break;
        case GPG_ERR_NO_PUBKEY:
          stream_
              << tr("A signature could NOT be verified due to a Missing Key")
              << Qt::endl;
          setStatus(-2);
          print_signer_without_key(stream_, GpgSignature(sign));
          break;
        case GPG_ERR_CERT_REVOKED:
          stream_ << tr("A signature is valid but the key used to verify the "
                        "signature has been revoked")
                  << Qt::endl;
          if (!print_signer(stream_, GpgSignature(sign))) {
            setStatus(0);
          }
          setStatus(-1);
          break;
        case GPG_ERR_SIG_EXPIRED:
          stream_ << tr("A signature is valid but expired") << Qt::endl;
          if (!print_signer(stream_, GpgSignature(sign))) {
            setStatus(0);
          }
          setStatus(-1);
          break;
        case GPG_ERR_KEY_EXPIRED:
          stream_ << tr("A signature is valid but the key used to "
                        "verify the signature has expired.")
                  << Qt::endl;
          if (!print_signer(stream_, GpgSignature(sign))) {
            setStatus(0);
          }
          break;
        case GPG_ERR_GENERAL:
          stream_ << tr("There was some other error which prevented "
                        "the signature verification.")
                  << Qt::endl;
          status_ = -1;
          can_continue = false;
          break;
        default:
          auto fpr = QString(sign->fpr);
          stream_ << tr("Error for key with fingerprint") << " "
                  << GpgFrontend::BeautifyFingerprint(fpr);
          setStatus(-1);
      }
      stream_ << Qt::endl;
      sign = sign->next;
    }
    stream_ << Qt::endl;
  } else {
    stream_
        << "-> "
        << tr("Could not find information that can be used for verification.")
        << Qt::endl;
    setStatus(0);
    return;
  }
}

auto GpgFrontend::GpgVerifyResultAnalyse::print_signer_without_key(
    QTextStream &stream, GpgSignature sign) -> bool {
  stream_ << "- " << tr("Signed By") << "(" << tr("Fingerprint") << ")"
          << ": "
          << (sign.GetFingerprint().isEmpty() ? tr("<unknown>")
                                              : sign.GetFingerprint())
          << Qt::endl;
  stream << "- " << tr("Public Key Algo") << ": " << sign.GetPubkeyAlgo()
         << Qt::endl;
  stream << "- " << tr("Hash Algo") << ": " << sign.GetHashAlgo() << Qt::endl;
  stream << "- " << tr("Sign Date") << "(" << tr("UTC") << ")"
         << ": " << sign.GetCreateTime().toString() << Qt::endl;
  stream << "- " << tr("Sign Date") << "(" << tr("Localized") << ")"
         << ": " << QLocale().toString(sign.GetCreateTime()) << Qt::endl;
  return true;
}

auto GpgFrontend::GpgVerifyResultAnalyse::print_signer(QTextStream &stream,
                                                       GpgSignature sign)
    -> bool {
  auto fingerprint = sign.GetFingerprint();
  auto key = GpgFrontend::GpgKeyGetter::GetInstance().GetKey(fingerprint);
  if (key.IsGood()) {
    stream << "- " << tr("Signed By") << ": " << key.GetUIDs()->front().GetUID()
           << Qt::endl;

    auto subkeys = key.GetSubKeys();
    auto it = std::find_if(subkeys->begin(), subkeys->end(),
                           [fingerprint](const GpgSubKey &k) {
                             return k.GetFingerprint() == fingerprint;
                           });

    if (it != subkeys->end()) {
      auto &subkey = *it;
      if (subkey.GetFingerprint() != key.GetFingerprint()) {
        stream << "- " << tr("Key ID") << ": " << key.GetId() << " ("
               << tr("Subkey") << ")" << Qt::endl;
      } else {
        stream << "- " << tr("Key ID") << ": " << key.GetId() << " ("
               << tr("Primary Key") << ")" << Qt::endl;
      }
      stream << "- " << tr("Key Create Date") << ": "
             << QLocale().toString(subkey.GetCreateTime()) << Qt::endl;
    }

  } else {
    stream_ << "- " << tr("Signed By") << "(" << tr("Fingerprint") << ")"
            << ": " << (fingerprint.isEmpty() ? tr("<unknown>") : fingerprint)
            << Qt::endl;
    setStatus(0);
  }

  stream << "- " << tr("Public Key Algo") << ": " << sign.GetPubkeyAlgo()
         << Qt::endl;
  stream << "- " << tr("Hash Algo") << ": " << sign.GetHashAlgo() << Qt::endl;
  stream << "- " << tr("Sign Date") << "(" << tr("UTC") << ")"
         << ": " << sign.GetCreateTime().toString() << Qt::endl;
  stream << "- " << tr("Sign Date") << "(" << tr("Localized") << ")"
         << ": " << QLocale().toString(sign.GetCreateTime()) << Qt::endl;
  stream << Qt::endl;
  return key.IsGood();
}

auto GpgFrontend::GpgVerifyResultAnalyse::GetSignatures() const
    -> gpgme_signature_t {
  if (result_.IsGood()) {
    return result_.GetRaw()->signatures;
  }
  return nullptr;
}

auto GpgFrontend::GpgVerifyResultAnalyse::TakeChargeOfResult()
    -> GpgFrontend::GpgVerifyResult {
  return result_;
}

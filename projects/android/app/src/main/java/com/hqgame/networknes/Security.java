////////////////////////////////////////////////////////////////////////////////////////
//
// Multiness - NES/Famicom emulator written in C++
// Based on Nestopia emulator
//
// Copyright (C) 2016-2018 Le Hoang Quyen
//
// This file is part of Multiness.
//
// Multiness is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// Multiness is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Multiness; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
////////////////////////////////////////////////////////////////////////////////////////

package com.hqgame.networknes;

import android.util.Base64;

import java.security.KeyFactory;
import java.security.PublicKey;
import java.security.Signature;
import java.security.spec.X509EncodedKeySpec;

class Security {
    private static final String[] BASE64_DEV_PUB_KEYS = new String[]{
            // full version's pubkey
            "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAifCeKFuMmcsY4R4hCJqRKYl0Qm3zrK8N/jqf6foiCs3a3ZjtePmTC1mI9oCIY5XVIf4VNIncvoDvhBpOZSvFgCdbxRSwcFR4M3RYDmrjnsaPYhVkx0Mrc57EhL3P1CPy92r/sP/GozXkMSrrDQP2r2NonSpYrdczoqTjuWuJtmVHqA5AP6wRjuvUNO4LmUSSHeOz9/oVToVIpUi8MSdZUVZcIBg1/rYPEB6nq4aMNxCR9STtz+i1ma8ukh2qcZopd0V7DYQ9c66IfHiInlFHzkPCU3Sx6dwtmNvRbANRf+JnjJy94dMorNnXyGoW5HOUVozVbA9nvUvLCj0irf56MwIDAQAB",
            // gpg version's pubkey
            "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAk+czqNg0/KwSxnmWrZaOnoJcRJ09zJEVsOpFdKsCKFL6NvLxarhgmSr3gSvO5d3+JznV+oD9UXHmzv4FWPEcGZvS58Dkgd6UNPjedHsiwhddQbvY3pIz6QLdsieJmB+wHS2IFPoLDU1I3knWvbaaBadzyU6AnuxHSvJwOXX5KCAjpLoeq6kBkjktukrHfD/+G8VIEggMICr1+dOujjJsblmqsFGIE2sbXCh/4s0Grzt8ZEtc3MmGJV5+OlOqh7M49k7A1xHs906utQQE3NbzTjZ6GvobdzomiG7nPSKtn7uZ2Ak7+CvSAzVbMW5dUXQFzD1ZeMUU7uGkP0mQ7F3dywIDAQAB"
            };
    private static final String KEY_ALGO = "RSA";
    private static final String SIG_ALGO = "SHA1WithRSA";

    public static boolean verify(String base64Key, String signedData, String base64Signature) {
        try {
            PublicKey publicKey = convertToPublicKey(base64Key);

            byte[] signatureBytes = Base64.decode(base64Signature, Base64.DEFAULT);

            Signature sig = Signature.getInstance(SIG_ALGO);
            sig.initVerify(publicKey);
            sig.update(signedData.getBytes("UTF-8"));

            return sig.verify(signatureBytes);

        } catch (Exception e) {
            e.printStackTrace();
        }

        return false;
    }

    public static boolean verifyPurchase(String signedData, String base64Signature) {
        for (String key: BASE64_DEV_PUB_KEYS) {
            if (verify(key, signedData, base64Signature))
                return true;
        }
        return false;
    }

    private static PublicKey convertToPublicKey(String base64Key) throws Exception {
        byte[] keyBytes = Base64.decode(base64Key, Base64.DEFAULT);
        KeyFactory factory = KeyFactory.getInstance(KEY_ALGO);
        return factory.generatePublic(new X509EncodedKeySpec(keyBytes));
    }
}

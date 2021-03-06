/**
 * @file authentication_test.cpp UT for Sprout authentication module.
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2013  Metaswitch Networks Ltd
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version, along with the "Special Exception" for use of
 * the program along with SSL, set forth below. This program is distributed
 * in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details. You should have received a copy of the GNU General Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * The author can be reached by email at clearwater@metaswitch.com or by
 * post at Metaswitch Networks Ltd, 100 Church St, Enfield EN2 6BQ, UK
 *
 * Special Exception
 * Metaswitch Networks Ltd  grants you permission to copy, modify,
 * propagate, and distribute a work formed by combining OpenSSL with The
 * Software, or a work derivative of such a combination, even if such
 * copying, modification, propagation, or distribution would otherwise
 * violate the terms of the GPL. You must comply with the GPL in all
 * respects for all of the code used other than OpenSSL.
 * "OpenSSL" means OpenSSL toolkit software distributed by the OpenSSL
 * Project and licensed under the OpenSSL Licenses, or a work based on such
 * software and licensed under the OpenSSL Licenses.
 * "OpenSSL Licenses" means the OpenSSL License and Original SSLeay License
 * under which the OpenSSL Project distributes the OpenSSL toolkit software,
 * as those licenses appear in the file LICENSE-OPENSSL.
 */

extern "C" {
#include <pjlib-util.h>
}

#include <string>
#include <sstream>
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "siptest.hpp"
#include "utils.h"
#include "stack.h"
#include "analyticslogger.h"
#include "localstore.h"
#include "avstore.h"
#include "hssconnection.h"
#include "authentication.h"
#include "fakehssconnection.hpp"
#include "fakechronosconnection.hpp"
#include "md5.h"

using namespace std;
using namespace std;
using testing::StrEq;
using testing::ElementsAre;
using testing::MatchesRegex;
using testing::HasSubstr;
using testing::Not;

/// Fixture for AuthenticationTest.
class AuthenticationTest : public SipTest
{
public:
  static void SetUpTestCase()
  {
    SipTest::SetUpTestCase();

    _local_data_store = new LocalStore();
    _av_store = new AvStore(_local_data_store);
    _hss_connection = new FakeHSSConnection();
    _chronos_connection = new FakeChronosConnection();
    _analytics = new AnalyticsLogger(&PrintingTestLogger::DEFAULT);
    _acr_factory = new ACRFactory();
    pj_status_t ret = init_authentication("homedomain",
                                          _av_store,
                                          _hss_connection,
                                          _chronos_connection,
                                          _acr_factory,
                                          _analytics);
    ASSERT_EQ(PJ_SUCCESS, ret);
  }

  static void TearDownTestCase()
  {
    destroy_authentication();
    delete _acr_factory;
    delete _hss_connection;
    delete _chronos_connection;
    delete _analytics;
    delete _av_store;
    delete _local_data_store;

    SipTest::TearDownTestCase();
  }

  AuthenticationTest() : SipTest(&mod_auth)
  {
  }

  ~AuthenticationTest()
  {
  }

  /// Parses a WWW-Authenticate header to the list of parameters.
  void parse_www_authenticate(const std::string& www_auth_hdr,
                              std::map<std::string, std::string>& params)
  {
    std::string hdr = www_auth_hdr;
    ASSERT_THAT(hdr, MatchesRegex("WWW-Authenticate *: *Digest *.*"));
    hdr = hdr.substr(hdr.find_first_of(':') + 1);
    hdr = hdr.substr(hdr.find("Digest") + 6);

    // Should now be at the start of the parameter list (barring white-space).
    while (!hdr.empty())
    {
      std::string p;
      size_t i = hdr.find_first_of(',');
      if (i != string::npos)
      {
        p = hdr.substr(0, i);
        hdr = hdr.substr(i+1);
      }
      else
      {
        p = hdr;
        hdr = "";
      }

      i = p.find_first_of('=');
      if (i != string::npos)
      {
        std::string pname = p.substr(0,i);
        std::string pvalue = p.substr(i+1);
        Utils::trim(pname);
        Utils::trim(pvalue);
        if ((pvalue[0] == '"') && (pvalue[pvalue.length()-1] == '"'))
        {
          // Remove quotes around parameter.
          pvalue = pvalue.substr(1, pvalue.length()-2);
        }
        params[pname] = pvalue;
      }
      else
      {
        std::string pname = p;
        Utils::trim(pname);
        params[pname] = "";
      }
    }
  }


protected:
  static LocalStore* _local_data_store;
  static AvStore* _av_store;
  static ACRFactory* _acr_factory;
  static FakeHSSConnection* _hss_connection;
  static FakeChronosConnection* _chronos_connection;
  static AnalyticsLogger* _analytics;
};

LocalStore* AuthenticationTest::_local_data_store;
AvStore* AuthenticationTest::_av_store;
ACRFactory* AuthenticationTest::_acr_factory;
FakeHSSConnection* AuthenticationTest::_hss_connection;
FakeChronosConnection* AuthenticationTest::_chronos_connection;
AnalyticsLogger* AuthenticationTest::_analytics;

class AuthenticationMessage
{
public:
  string _method;
  string _user;
  string _domain;
  bool _auth_hdr;
  string _auth_user;
  string _auth_realm;
  string _nonce;
  string _nc;
  string _cnonce;
  string _qop;
  string _uri;
  string _response;
  string _algorithm;
  string _opaque;
  string _integ_prot;
  string _auts;
  string _key;
  bool _sos;
  string _extra_contact;

  AuthenticationMessage(std::string method) :
    _method(method),
    _user("6505550001"),
    _domain("homedomain"),
    _auth_hdr(true),
    _auth_user("6505550001@homedomain"),
    _auth_realm("homedomain"),
    _nonce(""),
    _nc(""),
    _cnonce(""),
    _uri("sip:homedomain"),
    _response(""),
    _algorithm("MD5"),
    _opaque(""),
    _integ_prot(""),
    _auts(""),
    _key(""),
    _sos(false),
    _extra_contact("")
  {
  }

  std::string hash2str(md5_byte_t* hash);

  void calculate_digest_response();

  string get();

};


std::string AuthenticationMessage::hash2str(md5_byte_t* hash)
{
  std::stringstream ss;
  for (int i = 0; i < 16; ++i)
  {
    ss << std::hex << std::setfill('0') << std::setw(2) << (unsigned short)hash[i];
  }
  return ss.str();
}


void AuthenticationMessage::calculate_digest_response()
{
  md5_state_t md5;
  md5_byte_t resp[16];

  std::string ha1;
  if (_algorithm == "AKAv1-MD5")
  {
    // Key is a plain text password, so convert to HA1
    md5_init(&md5);
    md5_append(&md5, (md5_byte_t*)_auth_user.data(), _auth_user.length());
    md5_append(&md5, (md5_byte_t*)":", 1);
    md5_append(&md5, (md5_byte_t*)_auth_realm.data(), _auth_realm.length());
    md5_append(&md5, (md5_byte_t*)":", 1);
    for (size_t ii = 0; ii < _key.length(); ii += 2)
    {
      md5_byte_t byte = pj_hex_digit_to_val(_key[ii]) * 16 +
                        pj_hex_digit_to_val(_key[ii+1]);
      md5_append(&md5, &byte, 1);
    }
    md5_finish(&md5, resp);
    ha1 = hash2str(resp);
  }
  else
  {
    // Key is already HA1.
    ha1 = _key;
  }

  // Calculate HA2
  md5_init(&md5);
  md5_append(&md5, (md5_byte_t*)_method.data(), _method.length());
  md5_append(&md5, (md5_byte_t*)":", 1);
  md5_append(&md5, (md5_byte_t*)_uri.data(), _uri.length());
  md5_finish(&md5, resp);
  std::string ha2 = hash2str(resp);

  // Calculate the response.
  md5_init(&md5);
  md5_append(&md5, (md5_byte_t*)ha1.data(), ha1.length());
  md5_append(&md5, (md5_byte_t*)":", 1);
  md5_append(&md5, (md5_byte_t*)_nonce.data(), _nonce.length());
  md5_append(&md5, (md5_byte_t*)":", 1);
  md5_append(&md5, (md5_byte_t*)_nc.data(), _nc.length());
  md5_append(&md5, (md5_byte_t*)":", 1);
  md5_append(&md5, (md5_byte_t*)_cnonce.data(), _cnonce.length());
  md5_append(&md5, (md5_byte_t*)":", 1);
  md5_append(&md5, (md5_byte_t*)_qop.data(), _qop.length());
  md5_append(&md5, (md5_byte_t*)":", 1);
  md5_append(&md5, (md5_byte_t*)ha2.data(), ha2.length());
  md5_finish(&md5, resp);
  _response = hash2str(resp);
}


string AuthenticationMessage::get()
{
  char buf[16384];

  if ((_response.empty()) &&
      (!_key.empty()))
  {
    // No response provided, but a key is provided, so calculate the response.
    calculate_digest_response();
  }

  int n = snprintf(buf, sizeof(buf),
                   "%1$s sip:%3$s SIP/2.0\r\n"
                   "Via: SIP/2.0/TCP 10.83.18.38:36530;rport;branch=z9hG4bKPjmo1aimuq33BAI4rjhgQgBr4sY5e9kSPI\r\n"
                   "Via: SIP/2.0/TCP 10.114.61.213:5061;received=23.20.193.43;branch=z9hG4bK+7f6b263a983ef39b0bbda2135ee454871+sip+1+a64de9f6\r\n"
                   "Max-Forwards: 68\r\n"
                   "Supported: outbound, path\r\n"
                   "To: <sip:%2$s@%3$s>\r\n"
                   "From: <sip:%2$s@%3$s>;tag=fc614d9c\r\n"
                   "Call-ID: OWZiOGFkZDQ4MGI1OTljNjlkZDkwNTdlMTE0NmUyOTY.\r\n"
                   "CSeq: 1 %1$s\r\n"
                   "Expires: 300\r\n"
                   "Allow: INVITE, ACK, CANCEL, OPTIONS, BYE, REFER, NOTIFY, MESSAGE, SUBSCRIBE, INFO\r\n"
                   "User-Agent: X-Lite release 5.0.0 stamp 67284\r\n"
                   "Contact: <sip:%2$s@uac.example.com:5060;rinstance=f0b20987985b61df;transport=TCP%4$s>\r\n"
                   "%5$s"
                   "Route: <sip:sprout.ut.cw-ngv.com;transport=tcp;lr>\r\n"
                   "%6$s"
                   "Content-Length: 0\r\n"
                   "\r\n",
                   /*  1 */ _method.c_str(),
                   /*  2 */ _user.c_str(),
                   /*  3 */ _domain.c_str(),
                   /*  4 */ (_sos) ? ";sos" : "",
                   /*  5 */ _extra_contact.empty() ? "" : _extra_contact.append("\r\n").c_str(),
                   /*  6 */ _auth_hdr ?
                              string("Authorization: Digest ")
                                .append((!_auth_user.empty()) ? string("username=\"").append(_auth_user).append("\", ") : "")
                                .append((!_auth_realm.empty()) ? string("realm=\"").append(_auth_realm).append("\", ") : "")
                                .append((!_nonce.empty()) ? string("nonce=\"").append(_nonce).append("\", ") : "")
                                .append((!_uri.empty()) ? string("uri=\"").append(_uri).append("\", ") : "")
                                .append((!_response.empty()) ? string("response=\"").append(_response).append("\", ") : "")
                                .append((!_opaque.empty()) ? string("opaque=\"").append(_opaque).append("\", ") : "")
                                .append((!_nc.empty()) ? string("nc=").append(_nc).append(", ") : "")
                                .append((!_cnonce.empty()) ? string("cnonce=\"").append(_cnonce).append("\", ") : "")
                                .append((!_qop.empty()) ? string("qop=").append(_qop).append(", ") : "")
                                .append((!_auts.empty()) ? string("auts=\"").append(_auts).append("\", ") : "")
                                .append((!_integ_prot.empty()) ? string("integrity-protected=\"").append(_integ_prot).append("\", ") : "")
                                .append((!_algorithm.empty()) ? string("algorithm=").append(_algorithm) : "")
                                .append("\r\n").c_str() :
                              ""
    );

  EXPECT_LT(n, (int)sizeof(buf));

  string ret(buf, n);
  //cout << ret <<endl;
  return ret;
}


TEST_F(AuthenticationTest, NoAuthorizationPort)
{
  // Test that the authentication module lets through all requests on ports
  // other than S-CSCF port.
  TransportFlow tp(TransportFlow::Protocol::TCP,
                   stack_data.icscf_port,
                   "10.83.18.37",
                   36531);
  AuthenticationMessage msg("INVITE");
  msg._auth_hdr = false;
  inject_msg(msg.get(), &tp);

  // Expect no response as Authentication module lets request through.
  ASSERT_EQ(0, txdata_count());
}


TEST_F(AuthenticationTest, NoAuthorizationNonReg)
{
  // Test that the authentication module lets through non-REGISTER requests
  // with no authorization header.
  AuthenticationMessage msg("INVITE");
  msg._auth_hdr = false;
  pj_bool_t ret = inject_msg_direct(msg.get());
  EXPECT_EQ(PJ_FALSE, ret);
}


TEST_F(AuthenticationTest, NoAuthorizationEmergencyReg)
{
  // Test that the authentication module lets through emergency REGISTER requests
  AuthenticationMessage msg("REGISTER");
  msg._auth_hdr = false;
  msg._sos = true;
  pj_bool_t ret = inject_msg_direct(msg.get());
  EXPECT_EQ(PJ_FALSE, ret);
}


TEST_F(AuthenticationTest, IntegrityProtected)
{
  // Test that the authentication module lets through REGISTER requests
  // with authorization headers indicating the request has been integrity
  // protected at the P-CSCF.  Note that these requests must not have a
  // response field in the authorization header, otherwise this will be
  // checked.
  pj_bool_t ret;

  AuthenticationMessage msg1("REGISTER");
  msg1._auth_hdr = true;
  msg1._integ_prot = "yes";
  ret = inject_msg_direct(msg1.get());
  EXPECT_EQ(PJ_FALSE, ret);

  AuthenticationMessage msg2("REGISTER");
  msg2._auth_hdr = true;
  msg2._integ_prot = "tls-yes";
  ret = inject_msg_direct(msg2.get());
  EXPECT_EQ(PJ_FALSE, ret);

  AuthenticationMessage msg3("REGISTER");
  msg3._auth_hdr = true;
  msg3._integ_prot = "ip-assoc-yes";
  ret = inject_msg_direct(msg3.get());
  EXPECT_EQ(PJ_FALSE, ret);
}


// Tests that authentication is needed on registers that have at least one non
// emergency contact
TEST_F(AuthenticationTest, AuthorizationEmergencyReg)
{
  _hss_connection->set_result("/impi/6505550001%40homedomain/av?impu=sip%3A6505550001%40homedomain",
                              "{\"digest\":{\"realm\":\"homedomain\",\"qop\":\"auth\",\"ha1\":\"12345678123456781234567812345678\"}}");

  // Test that the authentication is required for REGISTER requests with one non-emergency contact
  AuthenticationMessage msg("REGISTER");
  msg._auth_hdr = false;
  msg._sos = true;
  msg._extra_contact = "Contact: <sip:6505550001@uac.example.com:5060;rinstance=a0b20987985b61df;transport=TCP>";
  inject_msg_direct(msg.get());

  // Expect a 401 Not Authorized response.
  ASSERT_EQ(1, txdata_count());
  pjsip_tx_data* tdata = current_txdata();
  RespMatcher(401).matches(tdata->msg);
  free_txdata();

  _hss_connection->delete_result("/impi/6505550001%40homedomain/av?impu=sip%3A6505550001%40homedomain");
}


TEST_F(AuthenticationTest, DigestAuthSuccess)
{
  // Test a successful SIP Digest authentication flow.
  pjsip_tx_data* tdata;

  // Set up the HSS response for the AV query using a default private user identity.
  _hss_connection->set_result("/impi/6505550001%40homedomain/av?impu=sip%3A6505550001%40homedomain",
                              "{\"digest\":{\"realm\":\"homedomain\",\"qop\":\"auth\",\"ha1\":\"12345678123456781234567812345678\"}}");

  // Send in a REGISTER request with no authentication header.  This triggers
  // Digest authentication.
  AuthenticationMessage msg1("REGISTER");
  msg1._auth_hdr = false;
  inject_msg(msg1.get());

  // Expect a 401 Not Authorized response.
  ASSERT_EQ(1, txdata_count());
  tdata = current_txdata();
  RespMatcher(401).matches(tdata->msg);

  // Extract the nonce, nc, cnonce and qop fields from the WWW-Authenticate header.
  std::string auth = get_headers(tdata->msg, "WWW-Authenticate");
  std::map<std::string, std::string> auth_params;
  parse_www_authenticate(auth, auth_params);
  EXPECT_NE("", auth_params["nonce"]);
  EXPECT_EQ("auth", auth_params["qop"]);
  EXPECT_EQ("MD5", auth_params["algorithm"]);
  free_txdata();

  // Send a new REGISTER request with an authentication header including the
  // response.
  AuthenticationMessage msg2("REGISTER");
  msg2._algorithm = "MD5";
  msg2._key = "12345678123456781234567812345678";
  msg2._nonce = auth_params["nonce"];
  msg2._opaque = auth_params["opaque"];
  msg2._nc = "00000001";
  msg2._cnonce = "8765432187654321";
  msg2._qop = "auth";
  inject_msg(msg2.get());

  // Expect no response, as the authentication module has let the request through.
  ASSERT_EQ(0, txdata_count());

  _hss_connection->delete_result("/impi/6505550001%40homedomain/av?impu=sip%3A6505550001%40homedomain");
}


TEST_F(AuthenticationTest, DigestAuthFailBadResponse)
{
  // Test a failed SIP Digest authentication flow where the response is wrong.
  pjsip_tx_data* tdata;

  // Set up the HSS response for the AV query using a default private user identity.
  _hss_connection->set_result("/impi/6505550001%40homedomain/av?impu=sip%3A6505550001%40homedomain",
                              "{\"digest\":{\"realm\":\"homedomain\",\"qop\":\"auth\",\"ha1\":\"12345678123456781234567812345678\"}}");

  // Send in a REGISTER request with an authentication header, but with no
  // integrity protected parameter.  This triggers Digest authentication.
  AuthenticationMessage msg1("REGISTER");
  inject_msg(msg1.get());

  // Expect a 401 Not Authorized response.
  ASSERT_EQ(1, txdata_count());
  tdata = current_txdata();
  RespMatcher(401).matches(tdata->msg);

  // Extract the nonce, nc, cnonce and qop fields from the WWW-Authenticate header.
  std::string auth = get_headers(tdata->msg, "WWW-Authenticate");
  std::map<std::string, std::string> auth_params;
  parse_www_authenticate(auth, auth_params);
  EXPECT_NE("", auth_params["nonce"]);
  EXPECT_EQ("auth", auth_params["qop"]);
  EXPECT_EQ("MD5", auth_params["algorithm"]);
  free_txdata();

  // Send a new REGISTER request with an authentication header including a
  // bad response.
  AuthenticationMessage msg2("REGISTER");
  msg2._algorithm = "MD5";
  msg2._key = "12345678123456781234567812345678";
  msg2._nonce = auth_params["nonce"];
  msg2._opaque = auth_params["opaque"];
  msg2._nc = "00000001";
  msg2._cnonce = "8765432187654321";
  msg2._qop = "auth";
  msg2._response = "00000000000000000000000000000000";
  inject_msg(msg2.get());

  // Check 403 forbidden response.
  ASSERT_EQ(1, txdata_count());
  tdata = current_txdata();
  RespMatcher(403).matches(tdata->msg);
  free_txdata();

  _hss_connection->delete_result("/impi/6505550001%40homedomain/av?impu=sip%3A6505550001%40homedomain");
}


TEST_F(AuthenticationTest, DigestAuthFailBadIMPI)
{
  // Test a failed SIP Digest authentication flow where the IMPI is not found
  // in the database.
  pjsip_tx_data* tdata;

  // Set up the HSS response for the AV query using a default private user identity.
  _hss_connection->set_result("/impi/6505550001%40homedomain/av?impu=sip%3A6505550001%40homedomain",
                              "{\"digest\":{\"realm\":\"homedomain\",\"qop\":\"auth\",\"ha1\":\"12345678123456781234567812345678\"}}");

  // Send in a REGISTER request with an authentication header with a bad IMPI.
  AuthenticationMessage msg1("REGISTER");
  msg1._auth_hdr = true;
  msg1._auth_user = "unknown@homedomain";
  inject_msg(msg1.get());

  // Expect a 403 Forbidden response.
  ASSERT_EQ(1, txdata_count());
  tdata = current_txdata();
  RespMatcher(403).matches(tdata->msg);
  free_txdata();

  _hss_connection->delete_result("/impi/6505550001%40homedomain/av?impu=sip%3A6505550001%40homedomain");
}


TEST_F(AuthenticationTest, DigestAuthFailStale)
{
  // Test a failed SIP Digest authentication flow where the response is stale.
  pjsip_tx_data* tdata;

  // Set up the HSS response for the AV query the default private user identity.
  _hss_connection->set_result("/impi/6505550001%40homedomain/av?impu=sip%3A6505550001%40homedomain",
                              "{\"digest\":{\"realm\":\"homedomain\",\"qop\":\"auth\",\"ha1\":\"12345678123456781234567812345678\"}}");

  // Send in a REGISTER request with an authentication header with a response
  // to an old challenge.  The content of the challenge doesn't matter,
  // provided it has a response and a nonce that won't be found in the AV
  // store.
  AuthenticationMessage msg1("REGISTER");
  msg1._auth_hdr = true;
  msg1._algorithm = "MD5";
  msg1._key = "12345678123456781234567812345678";
  msg1._nonce = "abcdefabcdefabcdefabcdefabcdef";
  msg1._opaque = "123123";
  msg1._nc = "00000001";
  msg1._cnonce = "8765432187654321";
  msg1._qop = "auth";
  msg1._response = "00000000000000000000000000000000";
  inject_msg(msg1.get());

  // The authentication module should recognise this as a stale request and
  // respond with a challenge.
  ASSERT_EQ(1, txdata_count());
  tdata = current_txdata();
  RespMatcher(401).matches(tdata->msg);

  // Extract the nonce, nc, cnonce and qop fields from the WWW-Authenticate header.
  std::string auth = get_headers(tdata->msg, "WWW-Authenticate");
  std::map<std::string, std::string> auth_params;
  parse_www_authenticate(auth, auth_params);
  EXPECT_NE("", auth_params["nonce"]);
  EXPECT_EQ("auth", auth_params["qop"]);
  EXPECT_EQ("MD5", auth_params["algorithm"]);
  EXPECT_EQ("true", auth_params["stale"]);
  free_txdata();

  // Send a new REGISTER request with an authentication header including the
  // response.
  AuthenticationMessage msg2("REGISTER");
  msg2._algorithm = "MD5";
  msg2._key = "12345678123456781234567812345678";
  msg2._nonce = auth_params["nonce"];
  msg2._opaque = auth_params["opaque"];
  msg2._nc = "00000001";
  msg2._cnonce = "8765432187654321";
  msg2._qop = "auth";
  inject_msg(msg2.get());

  // Expect no response, as the authentication module has let the request through.
  ASSERT_EQ(0, txdata_count());

  _hss_connection->delete_result("/impi/6505550001%40homedomain/av?impu=sip%3A6505550001%40homedomain");
}


TEST_F(AuthenticationTest, DigestAuthFailWrongRealm)
{
  // Test a failed SIP Digest authentication flow where the response contains the wrong realm.
  pjsip_tx_data* tdata;

  // Set up the HSS response for the AV query using a default private user identity.
  _hss_connection->set_result("/impi/6505550001%40homedomain/av?impu=sip%3A6505550001%40homedomain",
                              "{\"digest\":{\"realm\":\"homedomain\",\"qop\":\"auth\",\"ha1\":\"12345678123456781234567812345678\"}}");

  // Send in a REGISTER request with no authentication header.  This triggers
  // Digest authentication.
  AuthenticationMessage msg1("REGISTER");
  msg1._auth_hdr = false;
  inject_msg(msg1.get());

  // Expect a 401 Not Authorized response.
  ASSERT_EQ(1, txdata_count());
  tdata = current_txdata();
  RespMatcher(401).matches(tdata->msg);

  // Extract the nonce, nc, cnonce and qop fields from the WWW-Authenticate header.
  std::string auth = get_headers(tdata->msg, "WWW-Authenticate");
  std::map<std::string, std::string> auth_params;
  parse_www_authenticate(auth, auth_params);
  EXPECT_NE("", auth_params["nonce"]);
  EXPECT_EQ("auth", auth_params["qop"]);
  EXPECT_EQ("MD5", auth_params["algorithm"]);
  free_txdata();

  // Send a new REGISTER request with an authentication header including the
  // response but the wrong realm.
  AuthenticationMessage msg2("REGISTER");
  msg2._algorithm = "MD5";
  msg2._key = "12345678123456781234567812345678";
  msg2._nonce = auth_params["nonce"];
  msg2._opaque = auth_params["opaque"];
  msg2._nc = "00000001";
  msg2._cnonce = "8765432187654321";
  msg2._qop = "auth";
  msg2._auth_realm = "otherdomain";
  inject_msg(msg2.get());

  // Check 401 Unauthorized response.
  ASSERT_EQ(1, txdata_count());
  tdata = current_txdata();
  RespMatcher(401).matches(tdata->msg);
  free_txdata();

  _hss_connection->delete_result("/impi/6505550001%40homedomain/av?impu=sip%3A6505550001%40homedomain");
}


TEST_F(AuthenticationTest, DigestAuthFailTimeout)
{
  // Test a failed SIP Digest authentication flows where homestead is overloaded,
  // and when it reports the HSS is overloaded
  pjsip_tx_data* tdata;

  // Set up the HSS response for the AV query using a default private user identity.
  _hss_connection->set_rc("/impi/6505550001%40homedomain/av?impu=sip%3A6505550001%40homedomain",
                           503);
  _hss_connection->set_rc("/impi/6505550002%40homedomain/av?impu=sip%3A6505550001%40homedomain",
                           504);

  // Send in a REGISTER request.
  AuthenticationMessage msg1("REGISTER");
  msg1._auth_hdr = true;
  msg1._auth_user = "6505550001@homedomain";
  inject_msg(msg1.get());

  // Expect a 504 Server Timeout response.
  ASSERT_EQ(1, txdata_count());
  tdata = current_txdata();
  RespMatcher(504).matches(tdata->msg);
  free_txdata();

  msg1._auth_user = "6505550002@homedomain";
  inject_msg(msg1.get());

  // Expect a 504 Server Timeout response.
  ASSERT_EQ(1, txdata_count());
  tdata = current_txdata();
  RespMatcher(504).matches(tdata->msg);
  free_txdata();

  _hss_connection->delete_rc("/impi/6505550001%40homedomain/av?impu=sip%3A6505550001%40homedomain");
  _hss_connection->delete_rc("/impi/6505550002%40homedomain/av?impu=sip%3A6505550001%40homedomain");
}


TEST_F(AuthenticationTest, AKAAuthSuccess)
{
  // Test a successful AKA authentication flow.
  pjsip_tx_data* tdata;

  // Set up the HSS response for the AV query using a default private user identity.
  // The keys in this test case are not consistent, but that won't matter for
  // the purposes of the test as Clearwater never itself runs the MILENAGE
  // algorithms to generate or extract keys.
  _hss_connection->set_result("/impi/6505550001%40homedomain/av/aka?impu=sip%3A6505550001%40homedomain",
                              "{\"aka\":{\"challenge\":\"87654321876543218765432187654321\","
                                        "\"response\":\"12345678123456781234567812345678\","
                                        "\"cryptkey\":\"0123456789abcdef\","
                                        "\"integritykey\":\"fedcba9876543210\"}}");

  // Send in a REGISTER request with an authentication header with
  // integrity-protected=no.  This triggers aka authentication.
  AuthenticationMessage msg1("REGISTER");
  msg1._integ_prot = "no";
  inject_msg(msg1.get());

  // Expect a 401 Not Authorized response.
  ASSERT_EQ(1, txdata_count());
  tdata = current_txdata();
  RespMatcher(401).matches(tdata->msg);

  // Extract the nonce, nc, cnonce and qop fields from the WWW-Authenticate header.
  std::string auth = get_headers(tdata->msg, "WWW-Authenticate");
  std::map<std::string, std::string> auth_params;
  parse_www_authenticate(auth, auth_params);
  EXPECT_EQ("87654321876543218765432187654321", auth_params["nonce"]);
  EXPECT_EQ("0123456789abcdef", auth_params["ck"]);
  EXPECT_EQ("fedcba9876543210", auth_params["ik"]);
  EXPECT_EQ("auth", auth_params["qop"]);
  EXPECT_EQ("AKAv1-MD5", auth_params["algorithm"]);
  free_txdata();

  // Send a new REGISTER request with an authentication header including the
  // response.
  AuthenticationMessage msg2("REGISTER");
  msg2._algorithm = "AKAv1-MD5";
  msg2._key = "12345678123456781234567812345678";
  msg2._nonce = auth_params["nonce"];
  msg2._opaque = auth_params["opaque"];
  msg2._nc = "00000001";
  msg2._cnonce = "8765432187654321";
  msg2._qop = "auth";
  msg2._integ_prot = "yes";
  inject_msg(msg2.get());

  // Expect no response, as the authentication module has let the request through.
  ASSERT_EQ(0, txdata_count());

  _hss_connection->delete_result("/impi/6505550001%40homedomain/av/aka?impu=sip%3A6505550001%40homedomain");
}


TEST_F(AuthenticationTest, AKAAuthFailBadResponse)
{
  // Test a failed AKA authentication flow where the response is wrong.
  pjsip_tx_data* tdata;

  // Set up the HSS response for the AV query using a default private user identity.
  // The keys in this test case are not consistent, but that won't matter for
  // the purposes of the test as Clearwater never itself runs the MILENAGE
  // algorithms to generate or extract keys.
  _hss_connection->set_result("/impi/6505550001%40homedomain/av/aka?impu=sip%3A6505550001%40homedomain",
                              "{\"aka\":{\"challenge\":\"87654321876543218765432187654321\","
                                        "\"response\":\"12345678123456781234567812345678\","
                                        "\"cryptkey\":\"0123456789abcdef\","
                                        "\"integritykey\":\"fedcba9876543210\"}}");

  // Send in a REGISTER request with an authentication header with
  // integrity-protected=no.  This triggers aka authentication.
  AuthenticationMessage msg1("REGISTER");
  msg1._integ_prot = "no";
  inject_msg(msg1.get());

  // Expect a 401 Not Authorized response.
  ASSERT_EQ(1, txdata_count());
  tdata = current_txdata();
  RespMatcher(401).matches(tdata->msg);

  // Extract the nonce, nc, cnonce and qop fields from the WWW-Authenticate header.
  std::string auth = get_headers(tdata->msg, "WWW-Authenticate");
  std::map<std::string, std::string> auth_params;
  parse_www_authenticate(auth, auth_params);
  EXPECT_EQ("87654321876543218765432187654321", auth_params["nonce"]);
  EXPECT_EQ("0123456789abcdef", auth_params["ck"]);
  EXPECT_EQ("fedcba9876543210", auth_params["ik"]);
  EXPECT_EQ("auth", auth_params["qop"]);
  EXPECT_EQ("AKAv1-MD5", auth_params["algorithm"]);
  free_txdata();

  // Send a new REGISTER request with an authentication header with an incorrect
  // response.
  AuthenticationMessage msg2("REGISTER");
  msg2._algorithm = "AKAv1-MD5";
  msg2._key = "12345678123456781234567812345678";
  msg2._nonce = auth_params["nonce"];
  msg2._opaque = auth_params["opaque"];
  msg2._nc = "00000001";
  msg2._cnonce = "8765432187654321";
  msg2._qop = "auth";
  msg2._response = "00000000000000000000000000000000";
  msg2._integ_prot = "yes";
  inject_msg(msg2.get());

  // Check 403 forbidden response.
  ASSERT_EQ(1, txdata_count());
  tdata = current_txdata();
  RespMatcher(403).matches(tdata->msg);
  free_txdata();

  _hss_connection->delete_result("/impi/6505550001%40homedomain/av/aka?impu=sip%3A6505550001%40homedomain");
}


TEST_F(AuthenticationTest, AKAAuthResyncSuccess)
{
  // Test an AKA authentication flow that initially fails because the client
  // sequence number if out of sync with the HSS sequence number.
  pjsip_tx_data* tdata;

  // Set up the HSS response for the AV query using a default private user identity.
  // The keys in this test case are not consistent, but that won't matter for
  // the purposes of the test as Clearwater never itself runs the MILENAGE
  // algorithms to generate or extract keys.
  _hss_connection->set_result("/impi/6505550001%40homedomain/av/aka?impu=sip%3A6505550001%40homedomain",
                              "{\"aka\":{\"challenge\":\"87654321876543218765432187654321\","
                                        "\"response\":\"12345678123456781234567812345678\","
                                        "\"cryptkey\":\"0123456789abcdef\","
                                        "\"integritykey\":\"fedcba9876543210\"}}");

  // Send in a REGISTER request with an authentication header with
  // integrity-protected=no.  This triggers aka authentication.
  AuthenticationMessage msg1("REGISTER");
  msg1._integ_prot = "no";
  inject_msg(msg1.get());

  // Expect a 401 Not Authorized response.
  ASSERT_EQ(1, txdata_count());
  tdata = current_txdata();
  RespMatcher(401).matches(tdata->msg);

  // Extract the nonce, nc, cnonce and qop fields from the WWW-Authenticate header.
  std::string auth = get_headers(tdata->msg, "WWW-Authenticate");
  std::map<std::string, std::string> auth_params;
  parse_www_authenticate(auth, auth_params);
  EXPECT_EQ("87654321876543218765432187654321", auth_params["nonce"]);
  EXPECT_EQ("0123456789abcdef", auth_params["ck"]);
  EXPECT_EQ("fedcba9876543210", auth_params["ik"]);
  EXPECT_EQ("auth", auth_params["qop"]);
  EXPECT_EQ("AKAv1-MD5", auth_params["algorithm"]);
  free_txdata();

  // Set up a second HSS response for the resync query from the authentication
  // module.
  _hss_connection->set_result("/impi/6505550001%40homedomain/av/aka?impu=sip%3A6505550001%40homedomain&autn=876543218765432132132132132132",
                              "{\"aka\":{\"challenge\":\"12345678123456781234567812345678\","
                                        "\"response\":\"87654321876543218765432187654321\","
                                        "\"cryptkey\":\"fedcba9876543210\","
                                        "\"integritykey\":\"0123456789abcdef\"}}");

  // Send a new REGISTER request with an authentication header with a correct
  // response, but with an auts parameter indicating the sequence number in
  // the nonce was out of sync.
  AuthenticationMessage msg2("REGISTER");
  msg2._algorithm = "AKAv1-MD5";
  msg2._key = "12345678123456781234567812345678";
  msg2._nonce = auth_params["nonce"];
  msg2._opaque = auth_params["opaque"];
  msg2._nc = "00000001";
  msg2._cnonce = "8765432187654321";
  msg2._qop = "auth";
  msg2._auts = "32132132132132";
  msg2._integ_prot = "yes";
  inject_msg(msg2.get());

  // Expect another 401 Not Authorized response with a new challenge.
  ASSERT_EQ(1, txdata_count());
  tdata = current_txdata();
  RespMatcher(401).matches(tdata->msg);

  // Extract the nonce, nc, cnonce and qop fields from the WWW-Authenticate header.
  auth = get_headers(tdata->msg, "WWW-Authenticate");
  auth_params.clear();
  parse_www_authenticate(auth, auth_params);
  EXPECT_EQ("12345678123456781234567812345678", auth_params["nonce"]);
  EXPECT_EQ("fedcba9876543210", auth_params["ck"]);
  EXPECT_EQ("0123456789abcdef", auth_params["ik"]);
  EXPECT_EQ("auth", auth_params["qop"]);
  EXPECT_EQ("AKAv1-MD5", auth_params["algorithm"]);
  free_txdata();

  // Send a new REGISTER request with an authentication header with a correct
  // response to the second challenge.
  AuthenticationMessage msg3("REGISTER");
  msg3._algorithm = "AKAv1-MD5";
  msg3._key = "87654321876543218765432187654321";
  msg3._nonce = auth_params["nonce"];
  msg3._opaque = auth_params["opaque"];
  msg3._nc = "00000001";
  msg3._cnonce = "8765432187654321";
  msg3._qop = "auth";
  msg3._integ_prot = "yes";
  inject_msg(msg3.get());

  // Expect no response, as the authentication module has let the request through.
  ASSERT_EQ(0, txdata_count());

  _hss_connection->delete_result("/impi/6505550001%40homedomain/av/aka?impu=sip%3A6505550001%40homedomain&autn=876543218765432132132132132132");
  _hss_connection->delete_result("/impi/6505550001%40homedomain/av/aka?impu=sip%3A6505550001%40homedomain");
}


TEST_F(AuthenticationTest, AKAAuthResyncFail)
{
  // Test an AKA authentication flow that initially fails because the client
  // sequence number if out of sync with the HSS sequence number.  The resync
  // fails because the auts parameter is malformed.
  pjsip_tx_data* tdata;

  // Set up the HSS response for the AV query using a default private user identity.
  // The keys in this test case are not consistent, but that won't matter for
  // the purposes of the test as Clearwater never itself runs the MILENAGE
  // algorithms to generate or extract keys.
  _hss_connection->set_result("/impi/6505550001%40homedomain/av/aka?impu=sip%3A6505550001%40homedomain",
                              "{\"aka\":{\"challenge\":\"87654321876543218765432187654321\","
                                        "\"response\":\"12345678123456781234567812345678\","
                                        "\"cryptkey\":\"0123456789abcdef\","
                                        "\"integritykey\":\"fedcba9876543210\"}}");

  // Send in a REGISTER request with an authentication header with
  // integrity-protected=no.  This triggers aka authentication.
  AuthenticationMessage msg1("REGISTER");
  msg1._integ_prot = "no";
  inject_msg(msg1.get());

  // Expect a 401 Not Authorized response.
  ASSERT_EQ(1, txdata_count());
  tdata = current_txdata();
  RespMatcher(401).matches(tdata->msg);

  // Extract the nonce, nc, cnonce and qop fields from the WWW-Authenticate header.
  std::string auth = get_headers(tdata->msg, "WWW-Authenticate");
  std::map<std::string, std::string> auth_params;
  parse_www_authenticate(auth, auth_params);
  EXPECT_EQ("87654321876543218765432187654321", auth_params["nonce"]);
  EXPECT_EQ("0123456789abcdef", auth_params["ck"]);
  EXPECT_EQ("fedcba9876543210", auth_params["ik"]);
  EXPECT_EQ("auth", auth_params["qop"]);
  EXPECT_EQ("AKAv1-MD5", auth_params["algorithm"]);
  free_txdata();

  // Send a new REGISTER request with an authentication header with a correct
  // response, but with an auts parameter indicating the sequence number in
  // the nonce was out of sync.
  AuthenticationMessage msg2("REGISTER");
  msg2._algorithm = "AKAv1-MD5";
  msg2._key = "12345678123456781234567812345678";
  msg2._nonce = auth_params["nonce"];
  msg2._opaque = auth_params["opaque"];
  msg2._nc = "00000001";
  msg2._cnonce = "8765432187654321";
  msg2._qop = "auth";
  msg2._auts = "321321321321";    // Too short
  inject_msg(msg2.get());

  // Expect a 403 Forbidden response.
  ASSERT_EQ(1, txdata_count());
  tdata = current_txdata();
  RespMatcher(403).matches(tdata->msg);
  free_txdata();

  _hss_connection->delete_result("/impi/6505550001%40homedomain/av/aka?impu=sip%3A6505550001%40homedomain");
}


TEST_F(AuthenticationTest, AuthCorruptAV)
{
  // Test a handling of corrupt Authentication Vectors from Homestead.
  pjsip_tx_data* tdata;

  // Set up the HSS response for the AV query using a default private user
  // identity, with no aka or digest body.
  _hss_connection->set_result("/impi/6505550001%40homedomain/av/aka?impu=sip%3A6505550001%40homedomain",
                              "{}");

  // Send in a REGISTER request with an authentication header with
  // integrity-protected=no.  This triggers aka authentication.
  AuthenticationMessage msg1("REGISTER");
  msg1._integ_prot = "no";
  inject_msg(msg1.get());

  // Expect a 403 Forbidden response.
  ASSERT_EQ(1, txdata_count());
  tdata = current_txdata();
  RespMatcher(403).matches(tdata->msg);
  free_txdata();

  _hss_connection->delete_result("/impi/6505550001%40homedomain/av/aka?impu=sip%3A6505550001%40homedomain");

  // Set up the HSS response for the AV query using a default private user
  // identity, with a malformed aka body.
  _hss_connection->set_result("/impi/6505550001%40homedomain/av/aka?impu=sip%3A6505550001%40homedomain",
                              "{\"aka\":{\"challenge\":\"87654321876543218765432187654321\","
                                        "\"cryptkey\":\"0123456789abcdef\","
                                        "\"integritykey\":\"fedcba9876543210\"}}");

  // Send in a REGISTER request with an authentication header with
  // integrity-protected=no.  This triggers aka authentication.
  AuthenticationMessage msg2("REGISTER");
  msg2._integ_prot = "no";
  inject_msg(msg2.get());

  // Expect a 403 Forbidden response.
  ASSERT_EQ(1, txdata_count());
  tdata = current_txdata();
  RespMatcher(403).matches(tdata->msg);
  free_txdata();

  _hss_connection->delete_result("/impi/6505550001%40homedomain/av/aka?impu=sip%3A6505550001%40homedomain");

  // Set up the HSS response for the AV query the default private user identity,
  // with a malformed digest body.
  _hss_connection->set_result("/impi/6505550001%40homedomain/av?impu=sip%3A6505550001%40homedomain",
                              "{\"digest\":{\"realm\":\"homedomain\","
                                           "\"ha1\":\"12345678123456781234567812345678\"}}");

  // Send in a REGISTER request with no authentication header.  This triggers
  // Digest authentication.
  AuthenticationMessage msg3("REGISTER");
  msg3._auth_hdr = false;
  inject_msg(msg3.get());

  // Expect a 403 Forbidden response.
  ASSERT_EQ(1, txdata_count());
  tdata = current_txdata();
  RespMatcher(403).matches(tdata->msg);
  free_txdata();

  _hss_connection->delete_result("/impi/6505550001%40homedomain/av?impu=sip%3A6505550001%40homedomain");
}



#include "gsm.h"
#include "gsm_http.h"
#include "gsm_utilities.h"
#include "app_debug.h"
#include <stdio.h>
#include <string.h>

//#define m_http_packet_size 512
#define  POST_SSL_LEVEL              1

static gsm_http_config_t m_http_cfg;
static uint8_t m_http_step = 0;
static uint32_t m_total_bytes_recv = 0;
static uint32_t m_content_length = 0;
static char m_http_cmd_buffer[256];
//static uint32_t m_http_packet_size;
static gsm_http_data_t post_rx_data;

//static const char* root_ca = "-----BEGIN CERTIFICATE-----\n"
//"MIIDSjCCAjKgAwIBAgIQRK+wgNajJ7qJMDmGLvhAazANBgkqhkiG9w0BAQUFADA/\n"\
//"MSQwIgYDVQQKExtEaWdpdGFsIFNpZ25hdHVyZSBUcnVzdCBDby4xFzAVBgNVBAMT\n"\
//"DkRTVCBSb290IENBIFgzMB4XDTAwMDkzMDIxMTIxOVoXDTIxMDkzMDE0MDExNVow\n"\
//"PzEkMCIGA1UEChMbRGlnaXRhbCBTaWduYXR1cmUgVHJ1c3QgQ28uMRcwFQYDVQQD\n"\
//"Ew5EU1QgUm9vdCBDQSBYMzCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEB\n"\
//"AN+v6ZdQCINXtMxiZfaQguzH0yxrMMpb7NnDfcdAwRgUi+DoM3ZJKuM/IUmTrE4O\n"\
//"rz5Iy2Xu/NMhD2XSKtkyj4zl93ewEnu1lcCJo6m67XMuegwGMoOifooUMM0RoOEq\n"\
//"OLl5CjH9UL2AZd+3UWODyOKIYepLYYHsUmu5ouJLGiifSKOeDNoJjj4XLh7dIN9b\n"\
//"xiqKqy69cK3FCxolkHRyxXtqqzTWMIn/5WgTe1QLyNau7Fqckh49ZLOMxt+/yUFw\n"\
//"7BZy1SbsOFU5Q9D8/RhcQPGX69Wam40dutolucbY38EVAjqr2m7xPi71XAicPNaD\n"\
//"aeQQmxkqtilX4+U9m5/wAl0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNV\n"\
//"HQ8BAf8EBAMCAQYwHQYDVR0OBBYEFMSnsaR7LHH62+FLkHX/xBVghYkQMA0GCSqG\n"\
//"SIb3DQEBBQUAA4IBAQCjGiybFwBcqR7uKGY3Or+Dxz9LwwmglSBd49lZRNI+DT69\n"\
//"ikugdB/OEIKcdBodfpga3csTS7MgROSR6cz8faXbauX+5v3gTt23ADq1cEmv8uXr\n"\
//"AvHRAosZy5Q6XkjEGB5YGV8eAlrwDPGxrancWYaLbumR9YbK+rlmM6pZW87ipxZz\n"\
//"R8srzJmwN0jP41ZL9c8PDHIyh8bwRLtTcm1D9SZImlJnt1ir/md2cXjbDaJWFBM5\n"\
//"JDGFoqgCWjBH4d1QB7wCCZAA62RjYJsWvIjJEubSfZGL+T0yjWW06XyxV3bqxbYo\n"\
//"Ob8VZRzI9neWagqNdwvYkQsEjgfbKbYK7p2CNTUQ\n"\
//"-----END CERTIFICATE-----\n";

const char* root_ca = "-----BEGIN CERTIFICATE-----\r\n\
MIIDSjCCAjKgAwIBAgIQRK+wgNajJ7qJMDmGLvhAazANBgkqhkiG9w0BAQUFADA/\r\n\
MSQwIgYDVQQKExtEaWdpdGFsIFNpZ25hdHVyZSBUcnVzdCBDby4xFzAVBgNVBAMT\r\n\
DkRTVCBSb290IENBIFgzMB4XDTAwMDkzMDIxMTIxOVoXDTIxMDkzMDE0MDExNVow\r\n\
PzEkMCIGA1UEChMbRGlnaXRhbCBTaWduYXR1cmUgVHJ1c3QgQ28uMRcwFQYDVQQD\r\n\
Ew5EU1QgUm9vdCBDQSBYMzCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEB\r\n\
AN+v6ZdQCINXtMxiZfaQguzH0yxrMMpb7NnDfcdAwRgUi+DoM3ZJKuM/IUmTrE4O\r\n\
rz5Iy2Xu/NMhD2XSKtkyj4zl93ewEnu1lcCJo6m67XMuegwGMoOifooUMM0RoOEq\r\n\
OLl5CjH9UL2AZd+3UWODyOKIYepLYYHsUmu5ouJLGiifSKOeDNoJjj4XLh7dIN9b\r\n\
xiqKqy69cK3FCxolkHRyxXtqqzTWMIn/5WgTe1QLyNau7Fqckh49ZLOMxt+/yUFw\r\n\
7BZy1SbsOFU5Q9D8/RhcQPGX69Wam40dutolucbY38EVAjqr2m7xPi71XAicPNaD\r\n\
aeQQmxkqtilX4+U9m5/wAl0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNV\r\n\
HQ8BAf8EBAMCAQYwHQYDVR0OBBYEFMSnsaR7LHH62+FLkHX/xBVghYkQMA0GCSqG\r\n\
SIb3DQEBBQUAA4IBAQCjGiybFwBcqR7uKGY3Or+Dxz9LwwmglSBd49lZRNI+DT69\r\n\
ikugdB/OEIKcdBodfpga3csTS7MgROSR6cz8faXbauX+5v3gTt23ADq1cEmv8uXr\r\n\
AvHRAosZy5Q6XkjEGB5YGV8eAlrwDPGxrancWYaLbumR9YbK+rlmM6pZW87ipxZz\r\n\
R8srzJmwN0jP41ZL9c8PDHIyh8bwRLtTcm1D9SZImlJnt1ir/md2cXjbDaJWFBM5\r\n\
JDGFoqgCWjBH4d1QB7wCCZAA62RjYJsWvIjJEubSfZGL+T0yjWW06XyxV3bqxbYo\r\n\
Ob8VZRzI9neWagqNdwvYkQsEjgfbKbYK7p2CNTUQ\r\n\
-----END CERTIFICATE-----\r\n";

static const char *certificate_pem = "-----BEGIN CERTIFICATE-----\r\n\
MIIDXTCCAkWgAwIBAgIUW4vkdSVuQy9lnOovjKjRvc3QfwowDQYJKoZIhvcNAQEL\r\n\
BQAwPTELMAkGA1UEBhMCVk4xCzAJBgNVBAgMAkhOMSEwHwYDVQQKDBhJbnRlcm5l\r\n\
dCBXaWRnaXRzIFB0eSBMdGQwIBcNMjEwNjA5MDE0NjE1WhgPMjEyMTA1MTYwMTQ2\r\n\
MTVaMD0xCzAJBgNVBAYTAlZOMQswCQYDVQQIDAJITjEhMB8GA1UECgwYSW50ZXJu\r\n\
ZXQgV2lkZ2l0cyBQdHkgTHRkMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKC\r\n\
AQEAu0bKMttzgJAkmY4aD4kzxflCHR70lgqNQpujozx/GQ1ggE59RP4cnyqGjW+9\r\n\
YotQCTcFL0XeMONU5GwCIO9fZ7s2Y8n3/MFngWW/5D3TLy1yLi0/W97RHkQRHUqZ\r\n\
4sL0d91FbrQMZAEc9oT/Rpf+XaEsURFwiZN5ZuZEMMO3k8+pcWoX4M/3JsU4BBdy\r\n\
Dv4+Mtaqgk/jlSnirylZXLb6lcdOm5ZFJGC7mB7ltuvwMHcNXrgTyykxfvuikj1V\r\n\
0zLen95h99et42GTztJnzhRwx5ZV0TeKou889R3VfMqqgkG4gsVEhJwF3G82Jz8Z\r\n\
PY04WucITBhXTky6kvSJacapFwIDAQABo1MwUTAdBgNVHQ4EFgQUrLXVJVF3v7HI\r\n\
Uf4qohN4Az+ElwYwHwYDVR0jBBgwFoAUrLXVJVF3v7HIUf4qohN4Az+ElwYwDwYD\r\n\
VR0TAQH/BAUwAwEB/zANBgkqhkiG9w0BAQsFAAOCAQEAttYNEZ8As91wrT8pZdnn\r\n\
nbVcywUpDelRUuhOEtcHmc2qlB9H8hFxjFvakI4K1JN9lW23D3L6r7NoaoQiuIX9\r\n\
P3vx9h9G7qjetM1eHDuoliBs/MFp+DNJcp5ohydI/oDK4kyf50KQDYwp/bwP91Hp\r\n\
mYq41s84bwqBgENrgS8nXZPFpIx7nn8AdplZURi9+s61DP5IDvk812kUZspp9scy\r\n\
cLXnMBx6+jug5ZZR7BiWpVQJj8UdxOJxh9/BM7DD1KorfBP3MUFwMjSWvyFZwFm9\r\n\
yVPDO1YdvsLyH6d/zx7FsUQmyVR6TGPdhHmU1IXWJzuEb0J/DlPB5Y5WYABQEzpB\r\n\
Eg==\r\n\
-----END CERTIFICATE-----";

static const char *privkey = "-----BEGIN PRIVATE KEY-----\r\n\
MIIEvwIBADANBgkqhkiG9w0BAQEFAASCBKkwggSlAgEAAoIBAQC7Rsoy23OAkCSZ\r\n\
jhoPiTPF+UIdHvSWCo1Cm6OjPH8ZDWCATn1E/hyfKoaNb71ii1AJNwUvRd4w41Tk\r\n\
bAIg719nuzZjyff8wWeBZb/kPdMvLXIuLT9b3tEeRBEdSpniwvR33UVutAxkARz2\r\n\
hP9Gl/5doSxREXCJk3lm5kQww7eTz6lxahfgz/cmxTgEF3IO/j4y1qqCT+OVKeKv\r\n\
KVlctvqVx06blkUkYLuYHuW26/Awdw1euBPLKTF++6KSPVXTMt6f3mH3163jYZPO\r\n\
0mfOFHDHllXRN4qi7zz1HdV8yqqCQbiCxUSEnAXcbzYnPxk9jTha5whMGFdOTLqS\r\n\
9IlpxqkXAgMBAAECggEARdyikVZMQCmFfcME9ca5CaFyiGqD03UcPTzSTpLC1xWm\r\n\
ajbdhF9HThkPGLQWciyGLunXhUsLGDG1+YBRSvgBHzE3mQI/AIslkZ/jdcGahn7t\r\n\
mUxH1n3IhQHfYI3z2iPgDtb8j8+az7OamlwC3tLUkRkO7y8STEA3iatcxNQ+J2Us\r\n\
zUqQn0IcoizAKNI1ogoaCWLACVGIhOMAA4XfWhltDH0MnXMMOHXgwN6HzhZRQArF\r\n\
RTMCiTNXU9RXqJ0ZHoYqTBsuEUWKRmmpia1jBJICGeoaT1xZ3LegOubktYIKue2W\r\n\
zditaO86rl+h8KXBEoCsK5nkP98FjyPO6B0+9RCIAQKBgQDkh6lzodNfMlRCt1qe\r\n\
WuarGjYEtqacagtvVwn2M6jq7yAuhH2eozHwaF9Bylsl9d0mswkevf04fYEH0Ttg\r\n\
ddpibo83wUZL2gTe2EEuXHioTFWkhxd+mVIAdEVugCdwR9yRn8OZx9nQDgpRVFSl\r\n\
LnK37tcUf4OfI75hw1RVmIO4CQKBgQDRyazk0DbwtK2VPc1DEpZGfoZpsKvFl+Yb\r\n\
JngnLn3nC2feIymjXlAJm6bA8ZyGTxUngjbrCe1OtfbMYJ9tXlu+WMqI6PWeOb1g\r\n\
p+s6Qs/8SzVsVPILcKKhKv2ZnxR7RonV2HT7zzkgDR0EJprarJJ5Qcdozo6hkVO6\r\n\
h3TnOlBgHwKBgQDSVosAgtGprQkg3uHpHoFwuo89h1+SV4hu0g25LZMrqSxVpFx6\r\n\
xnoQbABA7Z83MTR7ig263eNTOzCnoUylW9PFBT2Mc7ff2Kri8OgNY88qGBg7dpuJ\r\n\
SlTPVjURn6KtFXdOEV5XDDrN5B5a/ONrpXSxFoOfuj3LG3r/QGk+30FdAQKBgQCw\r\n\
EIwz9LNHTLup1xZfxkesnh98sDNZP+R0wNJyP8iWkbH4cpZNb6fIiINoxt3QsqpU\r\n\
YCprFAe/2WNpn2XtyhVBKQ/B25HX2yme5w659LzNRultI9WH2F4E2SnBNgtgcpDX\r\n\
kjSL6RxOU/MYOrYX9GFxtsz+nuyBmJAmqexo6z3tjQKBgQChrKwXjx3B/sNL0mxE\r\n\
VWPm/+XGsJ/v7deUUws8xkJ9UoLUD905cqKZn9TJ4QYFGGNcLOcoE8gk4TDNj4NJ\r\n\
7ITe2Tlt8b1OOiGYXDKkqYoO01QTgkdj5a8fnuaxwV2KGyvqETJSKZ5YgGw19PUz\r\n\
Zt/byxPi3v8sFwXbAhUVRPxdaQ==\r\n\
-----END PRIVATE KEY-----\r\n";

static uint32_t m_ssl_step = 0;
static void setup_http_ssl(gsm_response_event_t event, void *response_buffer)
{
    switch (m_ssl_step)
    {
        case 0: // Set ssl
        {
            DEBUG_PRINTF("Enter setup ssl\n", 
                        (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", 
                        (char*)response_buffer);

            gsm_hw_send_at_cmd("AT+QHTTPCFG=\"sslctxid\",1\r\n", 
                                "OK\r\n", 
                                "", 
                                5000, 
                                2, 
                                setup_http_ssl);

            
        }
            break;
        
        case 1:
        {
            DEBUG_PRINTF("Set ssl config: %s, response %s\r\n", 
                        (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", 
                        (char*)response_buffer);

            gsm_hw_send_at_cmd("AT+QSSLCFG=\"sslversion\",1,4\r\n", 
                                "OK\r\n", 
                                "", 
                                5000, 
                                2, 
                                setup_http_ssl);

            
        }
            break;

        case 2:
        {
            DEBUG_PRINTF("Set the SSL version: %s, response %s\r\n", 
                        (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", 
                        (char*)response_buffer);

            gsm_hw_send_at_cmd("AT+QSSLCFG=\"ciphersuite\",1,0XFFFF\r\n", 
                                "OK\r\n", 
                                "", 
                                5000, 
                                2, 
                                setup_http_ssl);
        }
            break;
        default:
            break;
    }

    if (m_http_cfg.action == GSM_HTTP_ACTION_GET)
    //if (0)
    {
        switch (m_ssl_step)
        {
            case 3:
            {
                DEBUG_PRINTF("Set the SSL ciphersuite: %s, response %s\r\n", 
                            (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", 
                            (char*)response_buffer);

                gsm_hw_send_at_cmd("AT+QSSLCFG=\"seclevel\",1,0\r\n", 
                                    "OK\r\n", 
                                    "", 
                                    5000, 
                                    2, 
                                    setup_http_ssl);
            }
                break;
            
            case 4:
            {
                DEBUG_PRINTF("SSL level: %s, response %s\r\n", 
                            (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", 
                            (char*)response_buffer);
                gsm_hw_send_at_cmd("AT\r\n", 
                                    "OK\r\n", 
                                    "", 
                                    5000, 
                                    2, 
                                    gsm_http_query);
                m_ssl_step = 0;
                return;
            }   
                break;
        
        default:
            break;
        }
    }
    else
    {
        switch (m_ssl_step)
        {
            case 3:
            {
                DEBUG_PRINTF("Set the SSL ciphersuite: %s, response %s\r\n", 
                            (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", 
                            (char*)response_buffer);
#if (POST_SSL_LEVEL == 2)
                gsm_hw_send_at_cmd("AT+QSSLCFG=\"seclevel\",1,2\r\n", 
                                    "OK\r\n", 
                                    "", 
                                    5000, 
                                    2, 
                                    setup_http_ssl);
#else
                gsm_hw_send_at_cmd("AT+QSSLCFG=\"seclevel\",1,0\r\n", 
                                    "OK\r\n", 
                                    "", 
                                    5000, 
                                    2, 
                                    setup_http_ssl);
#endif
            }
                break;
            
            case 4:
            {
                DEBUG_PRINTF("SSL level: %s, response %s\r\n", 
                            (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", 
                            (char*)response_buffer);
                sprintf(m_http_cmd_buffer, "AT+QFUPL=\"RAM:cacert1.pem\",%u\r\n", strlen(root_ca));
                gsm_hw_send_at_cmd(m_http_cmd_buffer, 
                                    "CONNECT\r\n", 
                                    "", 
                                    5000, 
                                    1, 
                                    setup_http_ssl);
            }   
                break;

            case 5:
            {
                DEBUG_PRINTF("SSL upload rootCA: %s, response %s\r\n", 
                            (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", 
                            (char*)response_buffer);
                gsm_hw_send_at_cmd((char*)root_ca, 
                                    "+QFUPL:", 
                                    "OK", 
                                    5000, 
                                    1, 
                                    setup_http_ssl);           
            }
                break;
            
            case 6:
            {
                DEBUG_PRINTF("SSL upload ca result: %s, response %s\r\n", 
                            (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", 
                            (char*)response_buffer);
                gsm_hw_send_at_cmd("AT+QSSLCFG=\"cacert\",1,\"RAM:cacert1.pem\"\r\n", 
                                    "OK\r\n", 
                                    "", 
                                    1000, 
                                    1, 
                                    setup_http_ssl);      
            }
                break;



            case 7:
            {
                DEBUG_PRINTF("AT+QSSLCFG=\"cacert\": %s, response %s\r\n", 
                            (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", 
                            (char*)response_buffer);
                if (POST_SSL_LEVEL == 2)
                {
                    sprintf(m_http_cmd_buffer, "AT+QFUPL=\"RAM:clientcert.pem\",%u\r\n", strlen(certificate_pem));
                    gsm_hw_send_at_cmd(m_http_cmd_buffer, 
                                        "CONNECT\r\n", 
                                        "", 
                                        5000, 
                                        1, 
                                        setup_http_ssl);
                }
                else
                {
                    gsm_hw_send_at_cmd("AT\r\n", 
                                    "OK\r\n", 
                                    "", 
                                    5000, 
                                    2, 
                                    gsm_http_query);
                    m_ssl_step = 0;
                    return;
                }

            }   
                break;

            case 8:
            {
                DEBUG_PRINTF("SSL upload clientcert: %s, response %s\r\n", 
                            (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", 
                            (char*)response_buffer);
                gsm_hw_send_at_cmd((char*)certificate_pem, 
                                    "+QFUPL:", 
                                    "OK", 
                                    5000, 
                                    1, 
                                    setup_http_ssl);           
            }
                break;
            
            case 9:
            {
                DEBUG_PRINTF("SSL upload certificate_pem result: %s, response %s\r\n", 
                            (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", 
                            (char*)response_buffer);
                gsm_hw_send_at_cmd("AT+QSSLCFG=\"clientcert\",1,\"RAM:clientcert.pem\"\r\n", 
                                    "OK\r\n", 
                                    "", 
                                    1000, 
                                    1, 
                                    setup_http_ssl);      
            }
                break;



            case 10:
            {
                DEBUG_PRINTF("QSSLCFG=\"clientcert\": %s, response %s\r\n", 
                            (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", 
                            (char*)response_buffer);
                sprintf(m_http_cmd_buffer, "AT+QFUPL=\"RAM:clientkey.pem\",%u\r\n", strlen(privkey));
                gsm_hw_send_at_cmd(m_http_cmd_buffer, 
                                    "CONNECT\r\n", 
                                    "", 
                                    5000, 
                                    1, 
                                    setup_http_ssl);
            }   
                break;

            case 11:
            {
                DEBUG_PRINTF("SSL upload privkey: %s, response %s\r\n", 
                            (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", 
                            (char*)response_buffer);
                gsm_hw_send_at_cmd((char*)privkey, 
                                    "+QFUPL:", 
                                    "OK", 
                                    5000, 
                                    1, 
                                    setup_http_ssl);           
            }
                break;
            
            case 12:
            {
                DEBUG_PRINTF("SSL upload privkey result: %s, response %s\r\n", 
                            (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", 
                            (char*)response_buffer);
                gsm_hw_send_at_cmd("AT+QSSLCFG=\"clientkey\",1,\"RAM:clientkey.pem\"\r\n", 
                                    "OK\r\n", 
                                    "", 
                                    1000, 
                                    1, 
                                    setup_http_ssl);      
            }
                break;


            case 13:
            {
                DEBUG_PRINTF("QSSLCFG: %s, response %s\r\n", 
                            (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", 
                            (char*)response_buffer);

                gsm_hw_send_at_cmd("AT\r\n", 
                                    "OK\r\n", 
                                    "", 
                                    5000, 
                                    2, 
                                    gsm_http_query);
                m_ssl_step = 0;
                return;
            }
                break;

        default:
            break;
        }
    }

    m_ssl_step++;
}

#if 0   // SIMCOM
void gsm_http_query(gsm_response_event_t event, void *response_buffer)
{
    //sys_ctx_t *ctx = sys_ctx();

    switch (m_http_step)
    {
        case 0:     // Config bearer profile
        {
            if (m_http_cfg.on_event_cb)
            {
                if (m_http_cfg.on_event_cb) m_http_cfg.on_event_cb(GSM_HTTP_EVENT_START, NULL);
            }

            DEBUG_PRINTF("Configure bearer profile step1\r\n");
            gsm_hw_send_at_cmd("AT+SAPBR=3,1,\"Contype\",\"GPRS\"\r\n", 
                                "OK\r\n", 
                                "", 
                                5000, 
                                2, 
                                gsm_http_query);
        }
        break;

        case 1:
        {
            DEBUG_PRINTF("Configure bearer profile step1 : %s, response %s\r\n",
                         (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]",
                         (char*)response_buffer);

            gsm_hw_send_at_cmd("AT+SAPBR=3,1,\"APN\",\"v-internet\"\r\n", 
                                "OK\r\n", 
                                "", 
                                5000, 
                                2, 
                                gsm_http_query);
        }
        break;

        case 2:
        {
            DEBUG_PRINTF("AT+SAPBR=3,1,APN,v-internet : %s, response %s\r\n", 
                                            (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", 
                                            (char*)response_buffer);

            gsm_hw_send_at_cmd("AT+SAPBR=1,1\r\n", 
                                "OK\r\n", 
                                "", 
                                5000, 
                                2, 
                                gsm_http_query);
        }
        break;

        case 3:
        {
            DEBUG_PRINTF("AT+SAPBR=1,1 : %s, response %s\r\n", 
                        (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]",
                         (char*)response_buffer);

            gsm_hw_send_at_cmd("AT+SAPBR=2,1\r\n", 
                                "OK\r\n",
                                "",
                                5000, 
                                2, 
                                gsm_http_query);
        }
        break;

        case 4:
        {
            DEBUG_PRINTF("AT+SAPBR=2,1 : %s, response %s\r\n", 
                        (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", 
                        (char*)response_buffer);

            gsm_hw_send_at_cmd("AT+HTTPINIT\r\n", 
                                "OK\r\n", 
                                "", 
                                5000, 
                                2, 
                                gsm_http_query);
        }
        break;

        case 5: // Initialize HTTP connection
        {
            DEBUG_PRINTF("HTTP init : %s, response %s\r\n", 
                        (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", 
                        (char*)response_buffer);
            if (event == GSM_EVENT_OK)
            {
                gsm_hw_send_at_cmd("AT+HTTPPARA=\"CID\",1\r\n", 
                                    "OK\r\n", 
                                    "", 
                                    5000, 
                                    2, 
                                    gsm_http_query);
            }
            else
            {
                // Goto case Close GPSR context
                gsm_hw_send_at_cmd("AT\r\n", 
                                    "OK\r\n", 
                                    "",  
                                    5000, 
                                    10, 
                                    gsm_http_query);
                m_http_step = 11;
            }
        }
        break;

        case 6:     // Set http url
        {
            DEBUG_PRINTF("HTTP CID : %s, response %s\r\n", 
                        (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", 
                        (char*)response_buffer);

            if (event == GSM_EVENT_OK)
            {
                snprintf(m_http_cmd_buffer, sizeof(m_http_cmd_buffer), "AT+HTTPPARA=\"URL\",\"%s\"\r\n", m_http_cfg.url);
                DEBUG_PRINTF("Set HTTP url %s\r\n", m_http_cmd_buffer);

                gsm_hw_send_at_cmd(m_http_cmd_buffer, 
                                    "OK\r\n", 
                                    "",
                                    5000, 
                                    2, 
                                    gsm_http_query);
            }
            else
            {
                // Goto case close http session
                m_http_step = 11;
                gsm_hw_send_at_cmd("AT\r\n", 
                                    "OK\r\n", 
                                    "", 
                                    5000, 
                                    2, 
                                    gsm_http_query);
            }
        }
        break;

        case 7:     // Set HTTP port
        {
            DEBUG_PRINTF("HTTP setting url : %s, response %s\r\n", 
                        (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", 
                        (char*)response_buffer);

            if (event == GSM_EVENT_OK)
            {
                //snprintf(m_http_cmd_buffer, sizeof(m_http_cmd_buffer), "AT+HTTPPARA=\"PORT\",%d\r\n", m_http_cfg.port);
                snprintf(m_http_cmd_buffer, sizeof(m_http_cmd_buffer), "%s", "AT\r\n");
                DEBUG_PRINTF("Set HTTP port %s\r\n", m_http_cmd_buffer);

                gsm_hw_send_at_cmd(m_http_cmd_buffer, 
                                    "OK\r\n", 
                                    "",
                                    5000, 
                                    2, 
                                    gsm_http_query);
            }
            else
            {
                // Goto case close http session
                m_http_step = 11;
                gsm_hw_send_at_cmd("AT\r\n", 
                                    "OK\r\n", 
                                    "", 
                                    5000, 
                                    2, 
                                    gsm_http_query);
            }
        }
            break;

        case 8:
        {
            if (strstr(m_http_cfg.url, "https"))
            {
                DEBUG_PRINTF("HTTP with ssl\r\n");
                gsm_hw_send_at_cmd("AT+HTTPSSL=1\r\n", 
                                    "OK\r\n", 
                                    "", 
                                    5000, 
                                    2, 
                                    gsm_http_query);
                
            }
            else
            {
                DEBUG_PRINTF("Normal http\r\n");
                gsm_hw_send_at_cmd("AT\r\n", 
                                    "OK\r\n", 
                                    "", 
                                    5000, 
                                    2, 
                                    gsm_http_query);
            }
        }
            break;
        case 9: // Start HTTP download
        {
            DEBUG_PRINTF("HTTP setting port : %s, response %s\r\n", 
                        (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", 
                        (char*)response_buffer);

            gsm_hw_send_at_cmd("AT+HTTPACTION=0\r\n", 
                            "+HTTPACTION: 0,", 
                            "\r\n", 
                            60000, 
                            1, 
                            gsm_http_query);
        }
        break;

        case 10:
        {
            DEBUG_PRINTF("HTTP action : %s, response %s\r\n", 
                        (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", 
                        (char*)response_buffer);

            if (event == GSM_EVENT_OK)
            {
                uint32_t http_response_code;

                if (gsm_utilities_parse_http_action_response((char*)response_buffer, &http_response_code, &m_content_length))
                {
                    if (m_http_cfg.on_event_cb)
                    {
                        if (m_http_cfg.on_event_cb) m_http_cfg.on_event_cb(GSM_HTTP_EVENT_CONNTECTED, &m_content_length);
                    }

                    m_total_bytes_recv = 0;
                    DEBUG_PRINTF("Content length %u\r\n", m_content_length);
                    gsm_hw_clear_non_at_serial_rx_buffer();
                    gsm_hw_clear_at_serial_rx_buffer();
                    sprintf(m_http_cmd_buffer, "AT+HTTPREAD=%d,%d\r\n", m_total_bytes_recv, m_http_packet_size);

                    gsm_hw_send_at_cmd(m_http_cmd_buffer, 
                                        "+HTTPREAD: ", 
                                        "OK\r\n", 
                                        40000, 
                                        1, 
                                        gsm_http_query); // Close a GPRS context.
                }
                else
                {
                    DEBUG_PRINTF("HTTP error\r\n");
                    m_http_step = 11;
                    gsm_hw_send_at_cmd("AT\r\n", 
                                        "OK\r\n", 
                                        "", 
                                        5000, 
                                        10, 
                                        gsm_http_query);
                }
            }
            else
            {
                DEBUG_PRINTF("Cannot download file\r\n");
                // Goto case close http session
                m_http_step = 11;
                gsm_hw_send_at_cmd("AT\r\n", 
                                    "OK\r\n",  
                                    "", 
                                    5000, 
                                    10, 
                                    gsm_http_query);

            }
        }
        break;
        
        case 11:     // Proccess httpread data
        {
            DEBUG_PRINTF("HTTP read : %s\r\n", (event == GSM_EVENT_OK) ? "OK" : "FAIL");
            if (event == GSM_EVENT_OK)
            {
                uint8_t *p_begin_data;
                int tmp_read = gsm_utilities_parse_httpread_msg((char*)response_buffer, &p_begin_data);
                if (tmp_read != -1)
                {
                    uint32_t skip_size_until_begin_data = p_begin_data - (uint8_t*)response_buffer;
                    int32_t total_waiting_bytes = tmp_read + 6 + skip_size_until_begin_data;       // 6 = \r\nOK\r\n at the end of data
                    DEBUG_PRINTF("Read %d bytes, data %d\r\n", total_waiting_bytes, tmp_read);
                    uint32_t bytes_to_read = 0;

                    //http_download_buffer_t holder;
                    //holder.index = 0;

                    uint32_t bytes_availble_in_serial;
                    uint8_t *raw;
                    bytes_availble_in_serial = gsm_hw_direct_read_at_command_rx_buffer(&raw, 
                                                                                        total_waiting_bytes);
                    if (bytes_availble_in_serial == total_waiting_bytes)
                    {
                        bytes_to_read += bytes_availble_in_serial;
                        
                        if (m_http_cfg.on_event_cb)
                        {
                            gsm_http_data_t data;
                            data.length = tmp_read;
                            data.data = p_begin_data;
                            if (m_http_cfg.on_event_cb) m_http_cfg.on_event_cb(GSM_HTTP_GET_EVENT_DATA, &data);
                        }

                        m_total_bytes_recv += tmp_read;   
                        if (m_total_bytes_recv == m_content_length)
                        {
                            DEBUG_PRINTF("All data received\r\n");

                            gsm_hw_send_at_cmd("AT\r\n", 
                                                "OK\r\n", 
                                                "",
                                                5000, 
                                                10, 
                                                gsm_http_query);
                        }
                        else
                        {
                            gsm_hw_clear_non_at_serial_rx_buffer();
                            gsm_hw_clear_at_serial_rx_buffer();
                            sprintf(m_http_cmd_buffer, "AT+HTTPREAD=%d,%d\r\n", m_total_bytes_recv, m_http_packet_size);

                            gsm_hw_send_at_cmd(m_http_cmd_buffer, "+HTTPREAD: ", 
                                                "OK\r\n",
                                                40000, 
                                                1, 
                                                gsm_http_query);
                            m_http_step--;
                        }
                    }
                    else
                    {
                        DEBUG_PRINTF("Invalid HTTP read response, close http session\r\n");
                        // Goto case close http session
                        m_http_step = 11;
                        gsm_hw_send_at_cmd("AT\r\n", 
                                            "OK\r\n", 
                                            "", 
                                            5000, 
                                            10, 
                                            gsm_http_query);
                        break;
                    }
                }
                else
                {
                    DEBUG_PRINTF("HTTP read error\r\n");
                    // Goto case close http session
                    m_http_step = 11;
                    gsm_hw_send_at_cmd("AT\r\n", 
                                        "OK\r\n", 
                                        "", 
                                        5000, 
                                        10, 
                                        gsm_http_query);
                }
            }
        }
            break;
        
        case 12: // close http session
        {
            DEBUG_PRINTF("Closing http session\r\n");
            gsm_hw_send_at_cmd("AT+HTTPTERM\r\n", 
                                "OK\r\n", 
                                "", 
                                3000, 
                                1, 
                                gsm_http_query);
        }

        case 13: // Close GPSR context
        {
            DEBUG_PRINTF("Closed HTTP session %s, response %s\r\n",
                        (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]",
                         (char*)response_buffer);

            gsm_hw_send_at_cmd("AT+SAPBR=0,1\r\n", 
                                "OK\r\n", 
                                "", 
                                3000, 
                                1, 
                                gsm_http_query); // Close a GPRS context.
        }
        break;

        case 14: // Close GPRS profile
        {
            DEBUG_PRINTF("Closing GPRS context %s, response %s\r\n",
                        (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]",
                         (char*)response_buffer);

            gsm_hw_send_at_cmd("AT\r\n", 
                                "OK\r\n", 
                                "", 
                                5000, 
                                10, 
                                gsm_http_query);
        }
        break;

        case 15:        // Download finished
        {
            bool status = false;
            if (m_total_bytes_recv == m_content_length 
                && m_total_bytes_recv)
            {
                status = true;
            }

            if (m_http_cfg.on_event_cb)
            {
                if (status)
                {
                    if (m_http_cfg.on_event_cb) m_http_cfg.on_event_cb(GSM_HTTP_GET_EVENT_FINISH_SUCCESS, &m_total_bytes_recv);
                }
                else
                {
                    if (m_http_cfg.on_event_cb) m_http_cfg.on_event_cb(GSM_HTTP_EVENT_FINISH_FAILED, &m_total_bytes_recv);
                }
            }
        }
            break;

        default:
            DEBUG_PRINTF("[%s] Unhandle step %d\r\n", __FUNCTION__, m_http_step);
            break;
    }
    m_http_step++;
}
#else   // Quectel

void gsm_http_close_on_error(void)
{
        // Goto case close http session
    if (m_http_cfg.on_event_cb)
    {
        m_total_bytes_recv = 0;
        if (m_http_cfg.on_event_cb) m_http_cfg.on_event_cb(GSM_HTTP_EVENT_FINISH_FAILED, &m_total_bytes_recv);
    }
}

void gsm_http_query(gsm_response_event_t event, void *response_buffer)
{
    //sys_ctx_t *ctx = sys_ctx();

    switch (m_http_step)
    {
        case 0:     // Config bearer profile
        {
            if (m_http_cfg.on_event_cb)
            {
                if (m_http_cfg.on_event_cb) m_http_cfg.on_event_cb(GSM_HTTP_EVENT_START, NULL);
            }

            DEBUG_PRINTF("Set PDP context as 1\r\n");
            gsm_hw_send_at_cmd("AT+QHTTPCFG=\"contextid\",1\r\n", 
                                "OK\r\n", 
                                "", 
                                5000, 
                                2, 
                                gsm_http_query);
        }
        break;

        case 1: // Allow to output HTTP response header
        {
            DEBUG_PRINTF("Set PDP context as 1 : %s, response %s\r\n",
                         (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]",
                         (char*)response_buffer);
            if (m_http_cfg.action == GSM_HTTP_ACTION_GET)
            {
                gsm_hw_send_at_cmd("AT+QHTTPCFG=\"responseheader\",0\r\n", 
                                    "OK\r\n", 
                                    "", 
                                    5000, 
                                    2, 
                                    gsm_http_query);
            }
            else
            {
                //gsm_hw_send_at_cmd("AT+QHTTPCFG=\"requestheader\",0\r\n", 
                //                    "OK\r\n", 
                //                    "", 
                //                    5000, 
                //                    2, 
                //                    gsm_http_query);

                gsm_hw_send_at_cmd("AT\r\n", 
                                    "OK\r\n", 
                                    "", 
                                    5000, 
                                    2, 
                                    gsm_http_query);
            }
        }
        break;

        case 2:
        {
            //if (m_http_cfg.action == GSM_HTTP_ACTION_GET)
            {
                DEBUG_PRINTF("AT+QHTTPCFG=\"header\",1 : %s, response %s\r\n", 
                                                (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", 
                                                (char*)response_buffer);
            }
            gsm_hw_send_at_cmd("AT+QICSGP=1,1,\"v-internet\",\"\",\"\",1\r\n", 
                                "OK\r\n", 
                                "", 
                                5000, 
                                2, 
                                gsm_http_query);
        }
        break;

        case 3:     // Activate context 1
        {
            DEBUG_PRINTF("AT+QICSGP=1,1,\"v-internet\",\"\",\"\",1 : %s, response %s\r\n", 
                        (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]",
                         (char*)response_buffer);

            //gsm_hw_send_at_cmd("AT+QIACT=1\r\n", 
            //                    "OK\r\n",
            //                    "",
            //                    5000, 
            //                    2, 
            //                    gsm_http_query);
            gsm_hw_send_at_cmd("AT\r\n", 
                                "OK\r\n",
                                "",
                                5000, 
                                2, 
                                gsm_http_query);
        }
        break;

        case 4:     // Query state of context
        {
            //DEBUG_PRINTF("AT+QIACT=1: %s, response %s\r\n", 
            //            (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", 
            //            (char*)response_buffer);

            DEBUG_PRINTF("AT: %s, response %s\r\n", 
                        (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", 
                        (char*)response_buffer);
            gsm_hw_send_at_cmd("AT+QIACT?\r\n", 
                                "OK\r\n", 
                                "", 
                                5000, 
                                2, 
                                gsm_http_query);
        }
        break;
        
        case 5: // Set ssl
        {
            DEBUG_PRINTF("AT+QIACT: %s, response %s\r\n", 
                        (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", 
                        (char*)response_buffer);
            m_ssl_step = 0;
            gsm_hw_send_at_cmd("AT\r\n", 
                                "OK\r\n", 
                                "", 
                                5000, 
                                2, 
                                setup_http_ssl);
        }
            break;

        case 6: // Set url
        {
            DEBUG_PRINTF("SSL : %s, response %s\r\n", 
                        (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", 
                        (char*)response_buffer);
            DEBUG_PRINTF("URL %s\r\n", m_http_cfg.url);
            if (event == GSM_EVENT_OK)
            {
                sprintf(m_http_cmd_buffer, "AT+QHTTPURL=%u,1000\r\n", strlen(m_http_cfg.url));
                gsm_hw_send_at_cmd(m_http_cmd_buffer, 
                                    "CONNECT\r\n", 
                                    "", 
                                    1000, 
                                    1, 
                                    gsm_http_query);
            }
            else
            {
                // Goto case close http session
                gsm_http_close_on_error();
                return;
            }
        }
        break;

        case 7:
        {
            DEBUG_PRINTF("URL : %s, response %s\r\n", 
                        (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", 
                        (char*)response_buffer);

            gsm_hw_send_at_cmd(m_http_cfg.url, 
                            "OK\r\n", 
                            "", 
                            5000, 
                            2, 
                            gsm_http_query);
        }
            break;
        
        case 8: // Start HTTP download
        {
            
            DEBUG_PRINTF("HTTP setting url : %s, response %s\r\n", 
                        (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", 
                        (char*)response_buffer);
            if (m_http_cfg.action == GSM_HTTP_ACTION_GET)       // GET
            {
                gsm_hw_send_at_cmd("AT+QHTTPGET=12\r\n", 
                                "+QHTTPGET: 0,", 
                                "\r\n", 
                                20000, 
                                1, 
                                gsm_http_query);
            }
            else        // POST
            {

                post_rx_data.action = m_http_cfg.action;
                m_http_cfg.on_event_cb(GSM_HTTP_POST_EVENT_DATA, &post_rx_data);
                DEBUG_PRINTF("Send post data\r\n"); 
                sprintf(m_http_cmd_buffer, "AT+QHTTPPOST=%u,30,30\r\n", post_rx_data.length);        // Set maximum 10s input time, and 10s response from server
                gsm_hw_send_at_cmd(m_http_cmd_buffer, 
                                    "CONNECT", 
                                    "", 
                                    30000, 
                                    1, 
                                    gsm_http_query);
            }
        }
        break;

        case 9:
        {
            DEBUG_PRINTF("HTTP action : %s, response %s\r\n", 
                        (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", 
                        (char*)response_buffer);

            if (m_http_cfg.action == GSM_HTTP_ACTION_GET)       // GET
            {
                if (event == GSM_EVENT_OK)
                {
                    uint32_t http_response_code;

                    if (gsm_utilities_parse_http_action_response((char*)response_buffer, 
                                                                &http_response_code, 
                                                                &m_content_length))
                    {
                        if (m_http_cfg.on_event_cb)
                        {
                            if (m_http_cfg.on_event_cb) m_http_cfg.on_event_cb(GSM_HTTP_EVENT_CONNTECTED, &m_content_length);
                        }

                        m_total_bytes_recv = m_content_length;
                        DEBUG_PRINTF("Content length %u\r\n", m_content_length);
                        sprintf(m_http_cmd_buffer, "%s", "AT+QHTTPREAD=30\r\n");
                        //sprintf(m_http_cmd_buffer, "%s", "AT+QHTTPREADFILE=\"RAM:1.txt\",80\r\n");
                        gsm_hw_send_at_cmd(m_http_cmd_buffer, 
                                            "CONNECT\r\n", 
                                            "QHTTPREAD: 0", 
                                            12000, 
                                            1, 
                                            gsm_http_query); // Close a GPRS context.
                    }
                    else
                    {
                        DEBUG_PRINTF("HTTP error\r\n");
                        // Goto case close http session
                        gsm_http_close_on_error();
                        return;
                    }
                }
                else
                {
                    DEBUG_PRINTF("Cannot download file\r\n");
                    // Goto case close http session
                    gsm_http_close_on_error();
                    return;
                }
            }
            else        // POST
            {
                DEBUG_PRINTF("Input http post, len %u\r\n", strlen((char*)post_rx_data.data));
                gsm_hw_send_at_cmd((char*)post_rx_data.data, 
                                    "QHTTPPOST: ", 
                                    "\r\n", 
                                    20000, 
                                    1, 
                                    gsm_http_query); // Close a GPRS context.

                //gsm_hw_send_at_cmd("AT+QHTTPREAD=80\r\n", 
                //                    "CONNECT\r\n", 
                //                    "QHTTPREAD: 0", 
                //                    12000, 
                //                    1, 
                //                    gsm_http_query); // Close a GPRS context.
            }
        }
        break;
        
        case 10:     // Proccess httpread data
        {
            DEBUG_PRINTF("HTTP response : %s, data %s\r\n", 
                            (event == GSM_EVENT_OK) ? "OK" : "FAIL",
                             (char*)response_buffer);

            if (m_http_cfg.action == GSM_HTTP_ACTION_GET)       // GET
            {
                if (event == GSM_EVENT_OK)
                {
                    char *p_begin = strstr((char*)response_buffer, "CONNECT\r\n");
                    p_begin += strlen("CONNECT\r\n");
                    char *p_end = strstr(p_begin, "+QHTTPREAD: 0");
                    
                    gsm_http_data_t rx_data;
                    rx_data.action = m_http_cfg.action;
                    rx_data.length = p_end - p_begin;
                    rx_data.data = (uint8_t*)p_begin;

                    if (m_http_cfg.on_event_cb) 
                        m_http_cfg.on_event_cb(GSM_HTTP_GET_EVENT_DATA, &rx_data);

                    if (m_http_cfg.on_event_cb) 
                        m_http_cfg.on_event_cb(GSM_HTTP_GET_EVENT_FINISH_SUCCESS, &m_total_bytes_recv);
                }
                else
                {
                    gsm_http_close_on_error();
                    return;
                }
            }
            else
            {
                DEBUG_PRINTF("HTTP finish\r\n");
                if (m_http_cfg.on_event_cb) 
                    m_http_cfg.on_event_cb(GSM_HTTP_POST_EVENT_FINISH_SUCCESS, NULL);
            }
        }
            break;
        
        default:
            DEBUG_PRINTF("[%s] Unhandle step %d\r\n", __FUNCTION__, m_http_step);
            break;
    }
    m_http_step++;
}
#endif
gsm_http_config_t *gsm_http_get_config(void)
{
    return &m_http_cfg;
}

void gsm_http_cleanup(void)
{
    m_http_step = 0;
    m_total_bytes_recv = 0;
    m_content_length = 0;
    memset(&m_http_cmd_buffer, 0, sizeof(m_http_cmd_buffer));
    memset(&m_http_cfg, 0, sizeof(m_http_cfg));
}

bool gsm_http_start(gsm_http_config_t *config)
{
    // ASSERT(config);
    int32_t size_allowed = gsm_hw_serial_at_cmd_rx_buffer_size() 
                            - strlen("\r\nOK\r\n+HTTPREAD: ") 
                            - strlen("\r\nOK\r\n") 
                            - 32;      // 32 is a tricky for HTTPREAD header
    
    if (size_allowed < 4)
    {
        DEBUG_PRINTF("Serial buffer is too small, please increase it\r\n");
        return false;
    }

    size_allowed = (size_allowed - size_allowed%4);
    //m_http_packet_size = size_allowed;

    gsm_http_cleanup();
    memcpy(&m_http_cfg, config, sizeof(gsm_http_config_t));
    gsm_hw_send_at_cmd("AT\r\n", 
                        "OK\r\n", 
                        "", 
                        1000, 
                        2, 
                        gsm_http_query);

    return true;
}

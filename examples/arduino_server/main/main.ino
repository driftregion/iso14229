#include <CAN.h>
#include "iso14229.h"
#include <stdarg.h>
#include <stdint.h>

UDSServer_t srv;
UDSISOTpC_t tp;

int send_can(const uint32_t arb_id, const uint8_t *data, const uint8_t size, void *ud) {
  CAN.beginPacket(arb_id);
  CAN.write(data, size);
  CAN.endPacket();
  return size;
}

static void CANRecv(UDSISOTpC_t *tp) {
    assert(tp);
    uint8_t buf[8];
    int len = CAN.parsePacket();
    if (len) {
        if (len > 8) {
            Serial.println("CAN packet too long, truncating");
            len = 8;
        }
        CAN.readBytes(buf, len);
        UDS_LOGI(__FILE__, "can recv\n");
        UDS_DBG_PRINTHEX(buf, len);
        if (CAN.packetId() == tp->phys_sa) {
          UDS_LOGI(__FILE__, "phys frame received\n");
          isotp_on_can_message(&tp->phys_link, buf, len);
        } else if (CAN.packetId() == tp->func_sa) {
          if (ISOTP_RECEIVE_STATUS_IDLE != tp->phys_link.receive_status) {
            UDS_LOGI(__FILE__, "func frame received but cannot process because link is not idle");
            return;
          }
          isotp_on_can_message(&tp->func_link, buf, len);
    }
  }
}

extern "C" int print_impl(const char *fmt, ...);

const UDSISOTpCConfig_t tp_cfg = {
    .source_addr=0x7E8,
    .target_addr=0x7E0,
    .source_addr_func=0x7DF,
    .target_addr_func=UDS_TP_NOOP_ADDR,
    .isotp_user_send_can=send_can,
    .isotp_user_get_ms=UDSMillis,
    .isotp_user_debug=NULL,
    .user_data=NULL,
};

int print_impl(const char *fmt, ...) {
  char buf[256];
  va_list args;
  va_start(args, fmt);
  int ret = vsnprintf(buf, sizeof(buf), fmt, args);
  Serial.print(buf);
  va_end(args);
  return ret;
}

uint8_t fn(UDSServer_t *srv, int ev, const void *arg) {
     Serial.print("Got event ");
   Serial.println(ev);
   switch(ev) {
    case UDS_EVT_Err:
    {
          UDSErr_t *p_err = (UDSErr_t *)arg;
      Serial.print("Err: ");
      Serial.println(*p_err);
      break;
    }
   }
}

void setup() {
  Serial.begin(9600);
  while (!Serial);

  if(!UDSServerInit(&srv)) {
    Serial.println("UDSServerInit failed");
    while(1);
  }

  if (!UDSISOTpCInit(&tp, &tp_cfg)) {
    Serial.println("UDSISOTpCInit failed");
    while(1);
  }
  
  srv.tp = &tp.hdl;
  srv.fn = fn;

  // start the CAN bus at 500 kbps
  if (!CAN.begin(500E3)) {
    Serial.println("Starting CAN failed!");
    while (1);
  }

  Serial.println("Arduino UDS Server: Setup Complete");
}

void loop() {
  UDSServerPoll(&srv);
  CANRecv(&tp);
}

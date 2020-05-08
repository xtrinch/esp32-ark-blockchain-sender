#include <Arduino.h>
#include <HTTPClient.h>
#include <arkClient.h>
#include <WiFi.h>
#include "transactions/transaction.hpp"
#include "identities/publickey.hpp"
#include "transactions/builders/builder.hpp"
#include <ArduinoJson.h>
#include <sstream>

using namespace std;

extern "C" {
  uint8_t temprature_sens_read();
}

// macro to string expansion
#define xstr(s) strs(s)
#define strs(s) #s

// Set LED_BUILTIN if it is not defined by Arduino framework
// #define LED_BUILTIN 13

Ark::Client::Connection<Ark::Client::Api> connection("https://dexplorer.ark.io", 8443);

// returns temp
int getTempSensorValue() {
  // use internal sensor for some test data
  float temp = (temprature_sens_read() - 32) / 1.8; // convert to C
  return temp;
}

// decodes wallet data and extracts nonce from format {"data":{"address":"...", "nonce":"1", ...}}	
int getNonce(string passphrase) {
  string address = Ark::Crypto::identities::PublicKey::fromPassphrase(passphrase.c_str()).toString();
  string walletData = connection.api.wallets.get(address.c_str());
  StaticJsonDocument<200> doc;
  deserializeJson(doc, walletData);
  int nonce = doc.getMember("data").getMember("nonce");
  return nonce;
}

// creates and signs transaction
Ark::Crypto::transactions::Transaction createTransaction(int sensorData, string sensorName) {
  string passphrase = xstr(WALLET_PASSPHRASE);

  float amount = 1; // 1 * 10e-07
  float fee = 1000000; // 0.05 * 10e-07
  int currNonce = getNonce(passphrase);

  stringstream oss;
  oss << sensorName;
  oss << ":";
  oss << sensorData;
  string sensorString = oss.str();

  const Ark::Crypto::transactions::Transaction transaction = Ark::Crypto::transactions::builder::Transfer()
          .network(30)
          .nonce(currNonce + 1)
          .fee(fee)
          .amount(amount)
          .recipientId("DPKAyxsa7s9K3kQ5av8wwn9JY9a5ezxh4g")
          .vendorField(sensorString)
          .sign(passphrase)
          .build();

  return transaction;
}

// sends transaction to ark API
void sendTransaction(Ark::Crypto::transactions::Transaction transaction) {
  string jsonTransaction = "{" 
      "\"transactions\":["
          "" + transaction.toJson() + ""
      "]"
  "}";

  Serial.println(jsonTransaction.c_str());
  string resp = connection.api.transactions.send(jsonTransaction);
  Serial.println(resp.c_str());
}

void setup()
{
  // setup serial monitor
  Serial.begin(9600);

  WiFi.begin(xstr(WIFI_SSID), xstr(WIFI_PASSWORD));

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi..");
  }
  
  Serial.println("Connected to WiFi");

  pinMode(LED_BUILTIN, OUTPUT);
}

void loop()
{
  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  delay(6000);

  Ark::Crypto::transactions::Transaction transaction = createTransaction(getTempSensorValue(), "temp");
  sendTransaction(transaction);
}
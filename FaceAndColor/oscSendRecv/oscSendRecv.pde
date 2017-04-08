/**
 * oscP5sendreceive by andreas schlegel
 * example shows how to send and receive osc messages.
 * oscP5 website at http://www.sojamo.de/oscP5
 */

import oscP5.*;
import netP5.*;

OscP5 oscP5;
NetAddress myRemoteLocation;

void setup() {
  size(400, 400);
  frameRate(25);
  /* start oscP5, listening for incoming messages at port 12000 */
  oscP5 = new OscP5(this, 3000);

  /* myRemoteLocation is a NetAddress. a NetAddress takes 2 parameters,
   	 * an ip address and a port number. myRemoteLocation is used as parameter in
   	 * oscP5.send() when sending osc packets to another computer, device,
   	 * application. usage see below. for testing purposes the listening port
   	 * and the port of the remote location address are the same, hence you will
   	 * send messages back to this sketch.
   	 */
  myRemoteLocation = new NetAddress("127.0.0.1", 3001);
}

float x, y;

void draw() {
  background(0);

  //ellipse(x * width, y * height, 40, 40);
}

void mousePressed() {
  OscMessage myMessage = new OscMessage("/finish");
  myMessage.add(123);

  oscP5.send(myMessage, myRemoteLocation);
}

/* incoming osc message are forwarded to the oscEvent method. */
void oscEvent(OscMessage theOscMessage) {
  /* print the address pattern and the typetag of the received OscMessage */
  print("### received an osc message.");
  print(" addrpattern: " + theOscMessage.addrPattern());
  println(" typetag: " + theOscMessage.typetag());

  if (theOscMessage.addrPattern().equals("/right")) {
    x = theOscMessage.get(0).floatValue();
    y = theOscMessage.get(1).floatValue();
    println("x: " + x);
    println("y: " + y);
  }
}
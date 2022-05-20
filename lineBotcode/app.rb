require 'sinatra'
require 'json'
require 'line/bot'
require 'rubygems'
require 'paho-mqtt'


class App < Sinatra::Base
get '/' do
  'TestBot'
end
userList = []
pumpOff = true


    client ||= Line::Bot::Client.new { |config|
      config.channel_secret = 'xxxx'
      config.channel_token = 'xxxx'
    }


clientmq = PahoMqtt::Client.new
clientmq.host = 'mqtt.netpie.io'
clientmq.username = 'xxxx'
clientmq.password = 'xxxx'
clientmq.client_id = 'xxxx'
clientmq.port = 1883

clientmq.connect

message_counter = 0
clientmq.on_message do |message|
  puts "Message recieved on topic: #{message.topic}\n>>> #{message.payload}"
  msg = JSON.parse(message.payload)
  if message.topic=="@msg/project/pump"
    if msg["PumpStatus"] == 1
      puts "---------------------ปั้มน้ำเปิด--------------------"
      pumpOff=false
      client.multicast(userList,{ type: 'text', text:"ปั้มน้ำเปิด"})
    elsif msg["PumpStatus"] == 0
      pumpOff=true
      puts "--------------------ปั้มน้ำปิด--------------------"
      client.multicast(userList, {type: 'text', text:"ปั้มน้ำปิด"})
      
    end
    
  elsif message.topic=="@msg/project/status"
    msg = JSON.parse(message.payload)
    puts msg
    content = "Temperature : #{msg["Temperature"]} °C\nBrightness : #{msg["Brightness"]} %\nPump-status : #{(msg["PumpStatus"]==0)? "ปิด": "เปิด"}"
    client.push_message(msg["userId"], {type:'text' , text: content})
  end
  # client.push_message(testUserid, {type:'text' , text:"HELLO"})
  
  message_counter += 1
end

waiting_suback = true
clientmq.on_suback do
  waiting_suback = false
  puts "Subscribed"
end

clientmq.subscribe("@msg/project/#")


post '/callback' do
  body = request.body.read
  
  signature = request.env['HTTP_X_LINE_SIGNATURE']
  unless client.validate_signature(body, signature)
    error 400 do 'Bad Request' end
  end

  events = client.parse_events_from(body)

  events.each do |event|
    case event
    when Line::Bot::Event::Message
      case event.type
      when Line::Bot::Event::MessageType::Text
      if (event.message['text'].downcase == "status")
        message = {"command": event.message['text'].downcase , "userId": event['source']['userId']}
        waiting_publish = true
        while waiting_publish
          if  clientmq.publish("@msg/project/command",JSON.generate(message))
            waiting_publish = false
          end
        end
      elsif (event.message['text'].downcase.gsub(/\s+/, "") == "openpump")
        if(pumpOff)
          pumpOff = false # ปั้มเปิดเเล้ว
          message = {"command": "pumpon" , "userId": event['source']['userId']}
          waiting_publish = true
          while waiting_publish
            if  clientmq.publish("@msg/project/command",JSON.generate(message))
              waiting_publish = false
            end
          end
        client.multicast(userList,{type: 'text', text:"ตอนนี้ปั้มน้ำกำลังเปิด"})
        else
          client.reply_message(event['replyToken'], {type: 'text', text:"ปั้มน้ำกำลังทำงาน"})
        end
      elsif (event.message['text'].downcase.gsub(/\s+/, "") == "closepump")
        if(pumpOff)
          client.reply_message(event['replyToken'], {type: 'text', text:"ปั้มน้ำปิดอยู่ในขณะนี้"})
        else
          pumpOff = true
          client.multicast(userList,{type: 'text', text:"ตอนนี้ปั้มน้ำกำลังปิด"})
          message = {"command": "pumpoff" , "userId": event['source']['userId']}
          waiting_publish = true
          while waiting_publish
            if  clientmq.publish("@msg/project/command",JSON.generate(message))
              waiting_publish = false
            end
          end
        end
      elsif (event.message['text'].downcase == "/regist")
        unless userList.include? event['source']['userId']
          userList << event['source']['userId']
          client.reply_message(event['replyToken'], {type: 'text', text:"ลงทะเบียนสำเร็จ \nพิมพ์ /help เพื่อดูคำสั่ง"})
        end
      elsif (event.message['text'].downcase == "/help")
        client.reply_message(event['replyToken'], {type: 'text', text:"'status' - Check plant crop status\n'open pump' - Open your water pump\n'close pump' - Close your water pump"})
      else
        client.reply_message(event['replyToken'], {type: 'text', text:"ไม่มีคำสั่งในรายการ ดูคำสั่งทั้งหมดกรุณาพิมพ์ /help"})
      end
      end
    end

    
  end
  'OK'

end
end

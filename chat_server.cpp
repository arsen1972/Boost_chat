//
// chat_server.cpp
// ~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2018 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <algorithm>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <fstream>
#include <istream>
#include <list>
#include <set>
#include <string>

#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/asio.hpp>

#include "chat_message.hpp"

#define PATH_OF_SAVE "chatLogPass.sav"
#include "json.hpp"

using json = nlohmann::json;
using std::cin;
using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::map;
using std::set;
using std::list;
using std::pair;
using std::ofstream;
using std::ifstream;
using std::memcpy;
using std::strlen;
using std::for_each;

using boost::asio::ip::tcp;

typedef std::deque<chat_message> chat_message_queue;

namespace MapLoginPass
{
  void loadMapLoginPass();
  void printMapLoginPass();
  void saveMapLoginPass();
  void addToMapLoginPass(string, string);
}
json j_LoginPass;

string loginFrom;
string passwordFrom;

class chat_session;
set<chat_session*> session_;
//----------------------------------------------------------------------

class chat_participant
{
public:
  chat_participant()
  { cout << "chat_participant()" << endl;
  }
  virtual ~chat_participant()
  { cout << "~chat_participant()" << endl;
  }
  virtual void deliver(const chat_message& msg) = 0;

  bool accepted_ = false;
  string login_;
  set<boost::shared_ptr<chat_participant>> participants_;

  void send_msg_to_all(string str_msg_)
  {
    char line[chat_message::max_body_length + 1];// = loginFrom.c_str();
    strcpy(line, str_msg_.c_str());

    chat_message msg;
    msg.body_length(strlen(line));
    memcpy(msg.body(), line, msg.body_length());
    msg.encode_header();

    for_each(participants_.begin(), participants_.end(), boost::bind(&chat_participant::deliver, _1, boost::ref(msg)));
  }
};

typedef boost::shared_ptr<chat_participant> chat_participant_ptr;

//----------------------------------------------------------------------

class chat_room
{
public:
  void join(chat_participant_ptr participant)
  {
    current_participant_ = *(participants_.insert(participant)).first;
    current_participant_->participants_ = participants_;
    for_each(recent_msgs_.begin(), recent_msgs_.end(), boost::bind(&chat_participant::deliver, participant, _1));
  }

  void deliver(const chat_message& msg)
  {
    string srvMsg(msg.body(), msg.body_length());
  
    if (current_participant_->accepted_ == false)
    {
      if(0 == srvMsg.find(":setusername:"))
      { loginFrom = srvMsg.substr(13);
      }
      
      if(0 == srvMsg.find(":setpassword:"))
      { passwordFrom = srvMsg.substr(13);
        cout << "login-> " << loginFrom << " : " <<  "password-> " << passwordFrom << endl;

        bool isLoginInJ_LoginPass = false;

        for(auto el : j_LoginPass.items())
        { if (el.key() == loginFrom)
          { isLoginInJ_LoginPass = true;
        } }

        if(!isLoginInJ_LoginPass)
        {
          cout << "This login no used. Do your want authorization with this login&pass? (Y/~)"<< endl;

          string answerAuthorization;
          while (answerAuthorization.empty())
          { getline(cin, answerAuthorization);
          }
          
          if (answerAuthorization == "Y" || answerAuthorization == "y")
          {
            MapLoginPass::addToMapLoginPass(loginFrom, passwordFrom);
            current_participant_->accepted_ = true;
            current_participant_->login_ = loginFrom;
            
            char line[chat_message::max_body_length + 1];
            
            strcpy(line,loginFrom.c_str());
            strncat(line, ", welcome to chat!", chat_message::max_body_length + 1);

            current_participant_->send_msg_to_all(loginFrom + " join to chat");

            chat_message msg;
            msg.body_length(strlen(line));
            memcpy(msg.body(), line, msg.body_length());
            msg.encode_header();
            current_participant_->deliver(msg);
          }

          else 
          {
            char line[chat_message::max_body_length + 1];
            strcpy(line,loginFrom.c_str());
            strncat(line, ", access to the chat is closed for you! Goodbay!", chat_message::max_body_length + 1); 

            chat_message msg;
            msg.body_length(strlen(line));
            memcpy(msg.body(), line, msg.body_length());
            msg.encode_header();
            current_participant_->deliver(msg);

            leave(current_participant_);
          }
        }
                
        else if (j_LoginPass.at(loginFrom) == passwordFrom)
        {
          cout << "Good login password" << endl;
          current_participant_->accepted_ = true;
          current_participant_->login_ = loginFrom;

          char line[chat_message::max_body_length + 1];
          strcpy(line,loginFrom.c_str());
          strncat(line, ", welcome to chat!", chat_message::max_body_length + 1);

          current_participant_->send_msg_to_all(loginFrom + " join to chat");

          chat_message msg;
          msg.body_length(strlen(line));
          memcpy(msg.body(), line, msg.body_length());
          msg.encode_header();
          current_participant_->deliver(msg);
        }
        
        else
        {
          cout << "Bad login-password" << endl;
          current_participant_->accepted_ = false;

          char line[chat_message::max_body_length + 1];
          strcpy(line,loginFrom.c_str());
          strncat(line, ", bad login-password. Good Bye!", chat_message::max_body_length + 1);

          chat_message msg;
          msg.body_length(strlen(line));
          memcpy(msg.body(), line, msg.body_length());
          msg.encode_header();
          current_participant_->accepted_ = true;
          current_participant_->deliver(msg);
          current_participant_->accepted_ = false;

          cout << "leave(current_participant_); from bad login else" << endl;
          leave(current_participant_);
        }
    }
    }
    
    else
    { 
      recent_msgs_.push_back(msg);
      while (recent_msgs_.size() > max_recent_msgs)
        recent_msgs_.pop_front();
      for_each(participants_.begin(), participants_.end(), boost::bind(&chat_participant::deliver, _1, boost::ref(msg)));
    }
  }

  void leave(chat_participant_ptr participant)
  {
    current_participant_ = *(participants_.insert(participant)).first;
    current_participant_->participants_ = participants_;

    cout << "leave(chat_participant_ptr participant)" << endl;
    
    if (participant->accepted_)
    {
      participant->send_msg_to_all(participant->login_ + " leaved the chat....");
      participant->accepted_ = false;
    }
    participants_.erase(participant);
//    session_.clear();
  }

private:
  pair<set<chat_participant_ptr>::iterator, bool> participants_current_pair_;
  chat_participant_ptr current_participant_;
  set<chat_participant_ptr> participants_;
  enum { max_recent_msgs = 100 };
  chat_message_queue recent_msgs_;
//  public:
//  set<chat_session*> session_;
};

//----------------------------------------------------------------------

class chat_session : public chat_participant, public boost::enable_shared_from_this<chat_session>
{
public:
  chat_session(boost::asio::io_context& io_context, chat_room& room) : socket_(io_context), room_(room)
  { cout << "chat_session()" << endl;
    session_.insert(this);
    cout << "session_.size() = " << session_.size() << endl;
  }
  
  ~chat_session()
  {cout << "~chat_session()" << endl;
  }

  tcp::socket& socket()
  { return socket_;
  }

  void start()
  {
    room_.join(shared_from_this());
    boost::asio::async_read(socket_, boost::asio::buffer(read_msg_.data(), chat_message::header_length),
                            boost::bind( &chat_session::handle_read_header, shared_from_this(), boost::asio::placeholders::error));
  }

  void deliver(const chat_message& msg)
  {
    cout << "chat_session::deliver()" << endl;
    bool write_in_progress = !write_msgs_.empty();
    write_msgs_.push_back(msg);
    if (!write_in_progress)
    {
      boost::asio::async_write(socket_, boost::asio::buffer(write_msgs_.front().data(),
            write_msgs_.front().length()), boost::bind(&chat_session::handle_write, shared_from_this(), boost::asio::placeholders::error));
    }
  }

  void handle_read_header(const boost::system::error_code& error)
  {
    if (!error && read_msg_.decode_header())
    {
      boost::asio::async_read(socket_, boost::asio::buffer(read_msg_.body(), read_msg_.body_length()),
          boost::bind(&chat_session::handle_read_body, shared_from_this(), boost::asio::placeholders::error));
    }
    else
    {
      cout << "handle_read_header room_.leave(shared_from_this());" << std::endl;
      room_.leave(shared_from_this());
    }
  }

  void handle_read_body(const boost::system::error_code& error)
  {
    if (!error)
    {
      room_.deliver(read_msg_);
      boost::asio::async_read(socket_, boost::asio::buffer(read_msg_.data(), chat_message::header_length),
          boost::bind(&chat_session::handle_read_header, shared_from_this(), boost::asio::placeholders::error));
    }
    else
    {
        cout << "handle_read_body room_.leave(shared_from_this());" << std::endl;
        room_.leave(shared_from_this());
    }
  }

  void handle_write(const boost::system::error_code& error)
  {
    if (!error)
    {
      write_msgs_.pop_front();
      if (!write_msgs_.empty())
      {
        boost::asio::async_write(socket_, boost::asio::buffer(write_msgs_.front().data(),
              write_msgs_.front().length()), boost::bind(&chat_session::handle_write, shared_from_this(), boost::asio::placeholders::error));
      }
    }
    else
    {
      std::cout << "void handle_write room_.leave(shared_from_this());" << std::endl;
      room_.leave(shared_from_this());
    }
  }

private:
  tcp::socket socket_;
  chat_room& room_;
  chat_message read_msg_;
  chat_message_queue write_msgs_;
};

typedef boost::shared_ptr<chat_session> chat_session_ptr;

//----------------------------------------------------------------------

class chat_server
{
public:
  chat_server(boost::asio::io_context& io_context, const tcp::endpoint& endpoint) : io_context_(io_context), acceptor_(io_context, endpoint)
  { start_accept();
  }

  void start_accept()
  {
    chat_session_ptr new_session(new chat_session(io_context_, room_));
    acceptor_.async_accept(new_session->socket(), boost::bind(&chat_server::handle_accept, this, new_session, boost::asio::placeholders::error));
  }

  void handle_accept(chat_session_ptr session, const boost::system::error_code& error)
  {
    if (!error)
    { session->start();
    }
    start_accept();
  }

private:
  boost::asio::io_context& io_context_;
  tcp::acceptor acceptor_;
  chat_room room_;
};

typedef boost::shared_ptr<chat_server> chat_server_ptr;
typedef list<chat_server_ptr> chat_server_list;

//----------------------------------------------------------------------

int main(int argc, char* argv[])
{ 
  using namespace MapLoginPass;
  loadMapLoginPass();
//  addToMapLoginPass("test03", "test03p");
//  saveMapLoginPass();
  printMapLoginPass();
  
  try
  {
    if (argc < 2)
    { cerr << "Usage: chat_server <port> [<port> ...]\n";
      return 1;
    }

    boost::asio::io_context io_context;

    chat_server_list servers;
    for (int i = 1; i < argc; ++i)
    {
      tcp::endpoint endpoint(tcp::v4(), atoi(argv[i]));
      chat_server_ptr server(new chat_server(io_context, endpoint));
      servers.push_back(server);
    }

    io_context.run();
  }
  catch (std::exception& e)
  { cerr << "Exception: " << e.what() << "\n"; }

  return 0;
}
//***********************************************

//----------------------------------------------   loadMapLoginPass()
void MapLoginPass::loadMapLoginPass()
{
  string str;
  ifstream in(PATH_OF_SAVE);
  if (in.is_open())
  { while(getline(in, str))
    { j_LoginPass = json::parse(str);     
  } }

  in.close();
  return;
}
//----------------------------------------------   saveMapLoginPass()
void MapLoginPass::saveMapLoginPass()
{
  ofstream out(PATH_OF_SAVE, std::ios::out);
  if (out.is_open())
  { out << j_LoginPass << endl;
    cout << "SAVE: \n" << j_LoginPass << endl;
  }
  out.close();
  return;
}

//----------------------------------------------   addToMapLoginPass(string, string)
void MapLoginPass::addToMapLoginPass(string str_01, string str_02)
{
  j_LoginPass.emplace(str_01, str_02);
  saveMapLoginPass();
}
//----------------------------------------------   printMapLoginPass()
void MapLoginPass::printMapLoginPass()
{
  cout << "\nj_LoginPass: \n" << endl;
  for(auto el : j_LoginPass.items())
  { cout << el.key() << " -> " << el.value() << "\n";}
}

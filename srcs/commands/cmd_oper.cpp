#include "Server.hpp"

void Server::cmdTopic(Client &client, const Message &msg)
{
	if (!client.registered)
	{
		sendNumeric(client, "451", ":You have not registered");
		return;
	}
	if (msg.params.empty())
	{
		sendNumeric(client, "461", "TOPIC :Not enough parameters");
		return;
	}
	std::string name = ircLower(msg.params[0]);
	std::map<std::string, Channel>::iterator it = _channels.find(name);
	if (it == _channels.end())
	{
		sendNumeric(client, "403", name + " :No such channel");
		return;
	}
	Channel &chan = it->second;
	if (!chan.members.count(client.fd))
	{
		sendNumeric(client, "442", name + " :You're not on that channel");
		return;
	}
	if (msg.params.size() == 1)
	{
		if (chan.topic.empty())
			sendNumeric(client, "331", name + " :No topic is set");
		else
			sendNumeric(client, "332", name + " :" + chan.topic);
	}
	else
	{
		if (chan.topic_locked && !chan.operators.count(client.fd))
		{
			sendNumeric(client, "482", name + " :You're not channel operator");
			return;
		}
		chan.topic = msg.params[1];
		std::string line = clientPrefix(client) + " TOPIC " + name + " :" + chan.topic + "\r\n";
		sendToChannel(name, line, -1);
	}
}

void Server::cmdKick(Client &client, const Message &msg)
{
	if (!client.registered)
	{
		sendNumeric(client, "451", ":You have not registered");
		return;
	}
	if (msg.params.size() < 2)
	{
		sendNumeric(client, "461", "KICK :Not enough parameters");
		return;
	}
	std::string name = ircLower(msg.params[0]);
	std::map<std::string, Channel>::iterator it = _channels.find(name);
	if (it == _channels.end())
	{
		sendNumeric(client, "403", name + " :No such channel");
		return;
	}
	Channel &chan = it->second;
	if (!chan.members.count(client.fd))
	{
		sendNumeric(client, "442", name + " :You're not on that channel");
		return;
	}
	if (!chan.operators.count(client.fd))
	{
		sendNumeric(client, "482", name + " :You're not channel operator");
		return;
	}
	const std::string &target_nick = msg.params[1];
	Client *target_client = findClientByNick(target_nick);
	if (!target_client)
	{
		sendNumeric(client, "401", target_nick + " :No such nick");
		return;
	}
	if (!chan.members.count(target_client->fd))
	{
		sendNumeric(client, "441", target_nick + " " + name + " :They aren't on that channel");
		return;
	}
	std::string line = clientPrefix(client) + " KICK " + name + " " + target_client->nick;
	if (msg.params.size() > 2)
		line += " :" + msg.params[2];
	else
    	line += " :" + client.nick;
	line += "\r\n";
	sendToChannel(name, line, -1);
	std::string kicked = target_client->nick;
	chan.members.erase(target_client->fd);
	chan.operators.erase(target_client->fd);
	chan.invited.erase(target_client->fd);
	target_client->channels.erase(name);
	if (chan.members.empty())
		_channels.erase(it);
	logStatus(logTag("kick", CLR_RED) + " " + client.nick + " removed " + kicked + " from " + name);
}

void Server::cmdInvite(Client& client, const Message& msg)
{
	if (!client.registered)
	{
		sendNumeric(client, "451", ":You have not registered");
		return;
	}
	if (msg.params.size() < 2)
	{
		sendNumeric(client, "461", "INVITE :Not enough parameters");
		return;
	}
	std::string name = ircLower(msg.params[1]);
	std::map<std::string, Channel>::iterator it = _channels.find(name);
	if (it == _channels.end())
	{
		sendNumeric(client, "403", name + " :No such channel");
		return;
	}
	Channel &chan = it->second;
	if (!chan.members.count(client.fd))
	{
		sendNumeric(client, "442", name + " :You're not on that channel");
		return;
	}
	// only an invite-only channel restricts who may invite
	if (chan.invite_only && !chan.operators.count(client.fd))
	{
		sendNumeric(client, "482", name + " :You're not channel operator");
		return;
	}
	const std::string &target_nick = msg.params[0];
	Client *target_client = findClientByNick(target_nick);
	if (!target_client)
	{
		sendNumeric(client, "401", target_nick + " :No such nick");
		return;
	}
	// the opposite of KICK: here the target must NOT be in the channel yet
	if (chan.members.count(target_client->fd))
	{
		sendNumeric(client, "443", target_client->nick + " " + name + " :is already on channel");
		return;
	}
	// this is what the 473 check in cmdJoin reads, and what joining consumes
	chan.invited.insert(target_client->fd);
	// the inviter gets a confirmation and the invited one gets the invitation.
	// nobody else in the channel hears about it, an invite is private
	sendNumeric(client, "341", target_client->nick + " " + name);
	std::string line = clientPrefix(client) + " INVITE " + target_client->nick + " :" + name + "\r\n";
	sendToClient(target_client->fd, line);
}

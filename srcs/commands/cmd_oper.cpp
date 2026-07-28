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
	const std::string &name = msg.params[0];
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
	const std::string &name = msg.params[0];
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
	chan.members.erase(target_client->fd);
	chan.operators.erase(target_client->fd);
	chan.invited.erase(target_client->fd);
	target_client->channels.erase(name);
	if (chan.members.empty())
		_channels.erase(it);
}

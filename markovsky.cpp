#include <stdio.h>
#include <string.h>

#include "markovsky.h"
#include "markovutil.h"

int Markovsky::LoadSettings(void) {
  //Connect to DB
  try {
    this->sqlitecon = new sqlite::connection("markovsky.db");
    sqlite::execute(*this->sqlitecon, "CREATE TABLE IF NOT EXISTS lines(line TEXT PRIMARY KEY);", true);
  } catch(std::exception const &e) {
    std::cout << "An error occurred: " << e.what() << std::endl;
    return 1;
  }

  //Get lines from DB
  try {
    sqlite::query q(*this->sqlitecon, "SELECT line FROM lines;");
    boost::shared_ptr<sqlite::result> result = q.get_result();
    
    while (result->next_row()) {
      string line = result->get_string(0);
      std::cout << "Loading line: " << line << std::endl;
      this->Learn(line);
    }
  } catch(std::exception const &e) {
    std::cout << "An error occurred: " << e.what() << std::endl;
  }
  //cout << "boners" << endl;
  
  printf ("Parsed %i lines.\n", lines.size());
  printf ("I know %i words (%i contexts, %.2f per word), %i lines.\n", words.size(), 
      num_contexts, (float)num_contexts/(float)words.size(), lines.size());

  return true;
}

int Markovsky::SaveSettings(void) {
  set<string>::iterator it;
  sqlite::execute(*this->sqlitecon, "BEGIN TRANSACTION;", true);
  for (it = lines.begin(); it != lines.end(); ++it) {
    string qstr = "REPLACE INTO lines(line) VALUES(\"";
    qstr += (*it) + "\");";
    cout << qstr << endl;
    try {
      sqlite::execute(*this->sqlitecon, qstr, true);
    } catch(std::exception const &e) {
      std::cout << "An error occurred: " << e.what() << std::endl;
    }
  }
  sqlite::execute(*this->sqlitecon, "COMMIT;", true);
  return true;
}

string Markovsky::Reply(string message) {
  FilterMessage(message);
  string replystring;

  vector<string> curlines;
  splitString(message, curlines, ". ");
  vector<string> curwords;

  int sz, i;
  for (sz = curlines.size(), i = 0; i < sz; i++) {
	tokenizeString(curlines[i], curwords);
  }

  if (curwords.empty()) return replystring;

  // Filter out the words we don't know about
  int known = -1;
  vector<string> index;
  for (sz = curwords.size(), i = 0; i < sz; i++) {
	string &x = curwords[i];
	if (words.find(x) == words.end()) continue;
	int k = words[x].size();
	if ((known == -1) || (k < known)) {
	  index.clear();
	  index.push_back(x);
	  known = k;
	} else if (k == known) {
	  index.push_back(x);
	}
  }

  if (index.empty()) return replystring;
  
  deque<string> sentence;

  // pick a random word to start building the reply
  int x = rand() % (index.size());
  sentence.push_back(index[x]);

  // Build on the left edge
  bool done = false;
  while (!done) {
	// cline = line, w = word number
	int c = this->words[sentence[0]].size();
	context_t l = this->words[sentence[0]][rand() % (c)];
	int w = l.second;
	// NOTE: cline is needed since tokenizeString messes with its arguments
	string cline = *(l.first);	
	vector<string> cwords;
	tokenizeString (cline, cwords);
	
	int depth = rand() % (max_context_depth - min_context_depth) + min_context_depth;
	for (int i = 1; i <= depth; i++) {
	  if ((w - i) < 0) {
		done = true;
		break;
	  } else {
		sentence.push_front(cwords[w-i]);
	  }
	  if ((w - i) == 0) {
		done = true;
		break;
	  }
	}
  }

  // Build on the right edge
  done = false;
  while (!done) {
	if (words.find(sentence.back()) == words.end()) {
	  printf ("%s:%i: words.find(sentence.back()) == words.end()\n", __FILE__, __LINE__);
	}
	
	int c = this->words[sentence.back()].size();
	context_t l = this->words[sentence.back()][rand() % (c)];
	int w = l.second;
	string cline = *(l.first);
	vector<string> cwords;
	tokenizeString (cline, cwords);
	
	int depth = rand() % (max_context_depth - min_context_depth) + min_context_depth;
	for (int i = 1; i <= depth; i++) {
	  if ((w + i) >= cwords.size()) {
		done = true;
		break;
	  } else {
		sentence.push_back(cwords[w+i]);
	  }
	}
  }

  for (i = 0, sz = sentence.size() - 1; i < sz; i++) {
	replystring += sentence[i];
	replystring += ' ';
  }
  replystring += sentence.back();

  return replystring;
}


int Markovsky::Learn(string &body) {
  FilterMessage(body);
  vector<string> curlines;
  splitString(body, curlines, ". ");

  int sz = curlines.size();
  for (int i = 0; i < sz; i++) {
	LearnLine(curlines[i]);
  }
  
  return true;
}

int Markovsky::LearnLine(string &line) {
  // Ignore quotes
  if (isdigit(line[0])) return false;
  if (line[0] == '<') return false;
  if (line[0] == '[') return false;


  vector<string> curwords;
  tokenizeString(line, curwords);
  string cleanline = joinString(curwords);
  //if we already learned the line, return
  if (lines.find(cleanline) != lines.end()) return false; 

  set<string>::iterator lineit = lines.insert(cleanline).first;

  //loop through words contained in input, 
  int sz = curwords.size();
  for (int i = 0; i < sz; i++) {
	map<string, word_t>::iterator wit = words.find(curwords[i]);
  //if we've reached the last element, add the word to our brain
	if (wit == words.end()) {
	  word_t cword;
	  context_t cxt(lineit, i);
	  cword.push_back(cxt);
	  words[curwords[i]] = cword;
	} else {
	  context_t cxt(lineit, i);
	  ((*wit).second).push_back(cxt);
	}
	num_contexts++;
  }
  
  return true;
}


string Markovsky::ParseCommands(const string cmd) {
  if (cmd[0] != '!') return "";
  string command = cmd;
  lowerString(command);
  CMA_TokenizeString(command.c_str());
  //  CMA_TokenizeString(command.c_str());
  for (int i = 0; i < numbotcmds; i++) {
	if (!strncmp(CMA_Argv(0) + 1, botcmds[i].command, strlen(botcmds[i].command))) {
	  return botcmds[i].func(this, command);
	}
  }
  return "";
}

// Utility
// ---
int Markovsky::FilterMessage(string &message) {
  int n;	// MSVC doesn't like 'for' locality
  for (n = message.find('\n'); n != message.npos; n = message.find('\n', n)) {
	message.erase(n, 1);
  }
  
  for (n = message.find('\r'); n != message.npos; n = message.find('\r', n)) {
	message.erase(n, 1);
  }

  for (n = message.find('\"'); n != message.npos; n = message.find('\"', n)) {
	message.erase(n, 1);
  }

  for (n = message.find("? "); n != message.npos; n = message.find("? ", n)) {
	message.replace(n, 2, "?. ");
  }

  for (n = message.find("! "); n != message.npos; n = message.find("! ", n)) {
	message.replace(n, 2, "!. ");
  }

  lowerString (message);
  return true;
}

// ---------------------------------------------------------------------------

string CMD_Help_f(class Markovsky* self, const string command) {
  static string retstr;
  retstr = "Core Markovsky commands:\n";
  for (int i = 0; i < numbotcmds; i++) {
	retstr += "!";
	retstr += botcmds[i].command;
	retstr += ": ";
	retstr += botcmds[i].description;
	retstr += "\n";
  }
  return retstr;
}

string CMD_Version_f(class Markovsky* self, const string command) {
  return "I am Markovsky v" MARKOVSKYVERSIONSTRING;
}

string CMD_Words_f(class Markovsky* self, const string command) {
  static char retstr[4096];
  
  snprintf (retstr, 4096, "I know %i words (%i contexts, %.2f per word), %i lines.", 
			self->words.size(), self->num_contexts, 
			self->num_contexts/(float)self->words.size(),
			self->lines.size());
  return retstr;
}

string CMD_Known_f(class Markovsky* self, const string command) {
  if (CMA_Argc() < 2) return "Not enough parameters, usage: !known <word>";

  map<string, word_t>::iterator wit = self->words.find(CMA_Argv(1));
  static char retstr[4096];
  if (wit != self->words.end()) {
	int wordcontexts = ((*wit).second).size();
	snprintf (retstr, 4096, "%s is known (%i contexts)", CMA_Argv(1), wordcontexts);
  } else {
	snprintf (retstr, 4096, "%s is unknown", CMA_Argv(1));
  }
  
  return retstr;
}

string CMD_Ignore_f(class SeeBorg* self, const string command) {
  return "Not implemented yet";
}

string CMD_Top10_f(class SeeBorg* self, const string command) {
  return "Not implemented yet";
}

string CMD_Contexts_f(class Markovsky* self, const string command) {
  return "Not implemented yet";
}

string CMD_Unlearn_f(class Markovsky* self, const string command) {
  return "Not implemented yet";
}

string CMD_Replace_f(class Markovsky* self, const string command) {
  return "Not implemented yet";
}

string CMD_Quit_f(class Markovsky* self, const string command) {
  exit(0);

  return "Wow!";
}


/*
 * Alignments.cpp
 *
 *  Created on: May 25, 2012
 *      Author: fritz
 */

#include "Alignment.h"

void Alignment::setRef(string sequence) {
	alignment.second = sequence;
}
void Alignment::initAlignment() {
	al = new BamAlignment();
}
void Alignment::setAlignment(BamAlignment * align) {
	al = align;
	/*
	 //comment from here:
	 //todo: why does this influence the INV detection???
	 alignment.first.clear();
	 alignment.second.clear();
	 is_computed = false;

	 orig_length = al->QueryBases.size();

	 for (size_t i = 0; i < al->QueryBases.size(); i++) {
	 alignment.first += toupper(al->QueryBases[i]);
	 alignment.second += 'X';
	 }


	 stop = this->getPosition() + this->getRefLength();*/
}

void update_aln(std::string & alignment, int & i, int pos_to_modify) {
	int ref_pos = 0;
	while (i < alignment.size() && ref_pos != pos_to_modify) {
		if (alignment[i] != '-') {
			ref_pos++;
		}
		i++;
	}
	alignment[i] = 'Y';
}

void add_event(int pos, list<differences_str>::iterator & i, list<differences_str> & events) {
	//insert sorted into vector:
	while (i != events.end() && pos > (*i).position) {
		i++;
	}
	differences_str ev;
	ev.position = pos;
	ev.type = 0; //mismatch
	events.insert(i, ev);
}

void add_event(int pos, size_t & i, vector<differences_str> & events) {
	//insert sorted into vector:
	while (i < events.size() && pos > events[i].position) {
		i++;
	}
	differences_str ev;
	ev.position = pos;
	ev.type = 0; //mismatch
	events.insert(events.begin() + i, ev);
}
//todo: check if list changes things

vector<differences_str> Alignment::summarizeAlignment() {
	//clock_t comp_aln = clock();
	vector<differences_str> events;
	int pos = this->getPosition();
	differences_str ev;
	bool flag = false; // (strcmp(this->getName().c_str(), Parameter::Instance()->read_name.c_str()) == 0);

	for (size_t i = 0; i < al->CigarData.size(); i++) {
		if (al->CigarData[i].Type == 'D') {
			ev.position = pos;
			ev.type = al->CigarData[i].Length; //deletion
			events.push_back(ev);
			pos += al->CigarData[i].Length;
		} else if (al->CigarData[i].Type == 'I') {
			ev.position = pos;
			ev.type = al->CigarData[i].Length * -1; //insertion
			events.push_back(ev);
		} else if (al->CigarData[i].Type == 'M') {
			pos += al->CigarData[i].Length;
		} else if (al->CigarData[i].Type == 'N') {
			pos += al->CigarData[i].Length;
		} else if (al->CigarData[i].Type == 'S' && al->CigarData[i].Length > 4000) {
			string sa;
			al->GetTag("SA", sa);
			if (sa.empty()) { // == no split reads!
				if (flag) {
					std::cout << "Chop: " << pos << " Rname: " << this->getName() << std::endl;
				}
				if (pos == this->getPosition()) {
					ev.position = pos - Parameter::Instance()->huge_ins;
				} else {
					ev.position = pos;
				}
				ev.type = Parameter::Instance()->huge_ins * -1; //insertion: WE have to fix the length since we cannot estimate it!]
				events.push_back(ev);
			}
		}
	}

	if (flag) {
		for (size_t i = 0; i < events.size(); i++) {
			if (abs(events[i].type) > 3000) {
				cout << events[i].position << " " << events[i].type << endl;
			}
		}
		cout << endl;
	}

	//set ref length requ. later on:
	this->ref_len = pos - getPosition(); //TODO compare to get_length!

			//cout<<" comp len: "<<this->ref_len<<" "<<pos<<" "<<this->getPosition()<<endl;
//	Parameter::Instance()->meassure_time(comp_aln, "\t\tCigar: ");

	string md = this->get_md();
	pos = this->getPosition();
	int corr = 0;
	bool match = false;
	bool gap;
	int ref_pos = 0;
	size_t pos_events = 0;
	int max_size = (this->getRefLength() * 0.9) + getPosition();
	//comp_aln = clock();
	for (size_t i = 0; i < md.size() && pos < max_size; i++) {
		if (md[i] == '^') {
			gap = true;
		}
		if ((atoi(&md[i]) == 0 && md[i] != '0')) { //is not a number
			if (!gap) { // only mismatches are stored. We should have the rest from CIGAR
				//correct for shift in position with respect to the ref:
				while (ref_pos < events.size() && pos > events[ref_pos].position) {
					if (events[ref_pos].type > 0) {
						pos += events[ref_pos].type;
					}
					ref_pos++;
				}
				//store in sorted order:
				add_event(pos, pos_events, events);

				pos++; //just the pos on ref!
			}
			match = false;
		} else if (!match) {
			match = true;
			pos += atoi(&md[i]);
			gap = false;
		}
	}

	if (flag) {
		for (size_t i = 0; i < events.size(); i++) {
			if (abs(events[i].type) > 3000) {
				cout << events[i].position << " " << events[i].type << endl;
			}
		}
		cout << endl;
	}

	//Parameter::Instance()->meassure_time(comp_aln, "\t\tMD string: ");
//	comp_aln = clock();
	size_t i = 0;
	//to erase stretches of consecutive mismatches == N in the ref
	int break_point = 0;
	while (i < events.size()) {
		if (events[i].position > max_size) {
			while (i < events.size()) {
				if (abs(events[events.size() - 1].type) == Parameter::Instance()->huge_ins) {
					i++;
				} else {
					events.erase(events.begin() + i, events.begin() + i + 1);
				}
			}
			break;
		}
		if (events[i].type == 0) {
			size_t j = 1;
			while (i + j < events.size() && ((events[i + j].position - events[i + (j - 1)].position) == 1 && events[i + j].type == 0)) {
				j++;
			}
			if (j > 10) { //if stetch is at least 3 consecutive mismatches
				events.erase(events.begin() + i, events.begin() + i + j);
			} else {
				i += j;
			}
		} else {
			i++;
		}
	}
//	Parameter::Instance()->meassure_time(comp_aln, "\t\terrase N: ");
	if (flag) {
		cout << "LAST:" << endl;
		for (size_t i = 0; i < events.size(); i++) {
			if (abs(events[i].type) > 3000) {
				cout << events[i].position << " " << events[i].type << endl;
			}
		}
		cout << endl;
	}

	return events;
}
void Alignment::computeAlignment() {
	cout << "COMP ALN!" << endl;

	clock_t comp_aln = clock();
	int to_del = 0;
	int pos = 0;

	for (size_t i = 0; i < al->CigarData.size(); i++) {
		if (al->CigarData[i].Type == 'I') {
			to_del += al->CigarData[i].Length;
			alignment.second.insert(pos, al->CigarData[i].Length, '-');
			pos += al->CigarData[i].Length;
		} else if (al->CigarData[i].Type == 'D') {

			alignment.first.insert(pos, al->CigarData[i].Length, '-');
			alignment.second.insert(pos, al->CigarData[i].Length, 'X');
			pos += al->CigarData[i].Length;
			/*for (uint32_t t = 0; t < al->CigarData[i].Length; t++) {
			 alignment.first.insert(pos, "-");
			 alignment.second.insert(pos, "X");
			 pos++;
			 }*/
		} else if (al->CigarData[i].Type == 'S') {
			if (pos == 0) { //front side
				alignment.second.erase(((int) alignment.second.size()) - al->CigarData[i].Length, al->CigarData[i].Length);
			} else { //backside
				alignment.second.erase(pos, al->CigarData[i].Length);
			}
			alignment.first.erase(pos, al->CigarData[i].Length);
		} else if (al->CigarData[i].Type == 'M') {
			pos += al->CigarData[i].Length;
		} else if (al->CigarData[i].Type == 'H') {
			//nothing todo
		} else if (al->CigarData[i].Type == 'N') {
			alignment.second.erase(pos, al->CigarData[i].Length);
		}
	}
	if (to_del > 0) {
		alignment.second = alignment.second.substr(0, alignment.second.size() - to_del);
		//alignment.second.erase(alignment.second.size() - to_del, to_del);
	}
	Parameter::Instance()->meassure_time(comp_aln, "\t\tCIGAR opterations ");
	comp_aln = clock();
	//Apply MD string:
	string md = this->get_md();
	pos = 0;
	int corr = 0;
	bool match = false;
	int last_pos_string = 0;
	int last_pos_ref = 0;

	for (size_t i = 0; i < md.size(); i++) {
		if (atoi(&md[i]) == 0 && md[i] != '0') { //is not a number!
			if (md[i] != '^') {
				update_aln(alignment.second, last_pos_string, pos - last_pos_ref);
				last_pos_ref = pos;
				pos++;
			}
			match = false;
		} else if (!match) {
			match = true;
			pos += atoi(&md[i]);
		}
	}
	Parameter::Instance()->meassure_time(comp_aln, "\t\tMD opterations ");

	if (alignment.first.size() != alignment.second.size()) { // || strcmp(this->getName().c_str(),"IIIIII_10892000")==0) {
			//if(al->CigarData[0].Length!=100){
		cout << "Error alignment has different length" << endl;
		cout << " ignoring alignment " << al->Name << endl;
		cout << al->Position << endl;

		cout << endl;
		cout << "read: " << alignment.first << endl;
		cout << " ref: " << alignment.second << endl;
		cout << endl;
		cout << orig_length << endl;
		vector<CigarOp> cig = getCigar();
		for (size_t i = 0; i < cig.size(); i++) {
			cout << cig[i].Length << cig[i].Type << " ";
		}
		cout << endl;

		cout << this->get_md() << endl;

		//	exit(0);
		//	return;
	}
}
int32_t Alignment::getPosition() {
	return al->Position;
}
int32_t Alignment::getRefID() {
	return al->RefID;
}
bool Alignment::getStrand() {
	return !al->IsReverseStrand();
}
vector<CigarOp> Alignment::getCigar() {
	return al->CigarData;
}
string Alignment::getQualitValues() {
	return al->Qualities;
}
size_t Alignment::get_length(std::vector<CigarOp> CigarData) {
	size_t len = 0; //orig_length;
	for (size_t i = 0; i < CigarData.size(); i++) {
		if (CigarData[i].Type == 'D' || CigarData[i].Type == 'M' || CigarData[i].Type == 'N') {
			len += CigarData[i].Length;
		}
	}
	return len;
}
size_t Alignment::getRefLength() {
	return this->ref_len;
//	return get_length(this->al->CigarData);
}
size_t Alignment::getOrigLen() {
	return orig_length;
}
pair<string, string> Alignment::getSequence() {
	return alignment;
}
BamAlignment * Alignment::getAlignment() {
	return al;
}
string Alignment::getName() {
	return al->Name;
}
uint16_t Alignment::getMappingQual() {
	return al->MapQuality;
}

/*float Alignment::getIdentity() {
 if (is_computed) {
 float match = 0;
 for (size_t i = 0; i < alignment.first.size(); i++) {
 if (alignment.first[i] == alignment.second[i]) {
 match++;
 }
 }
 return match / (float) alignment.first.size();
 }
 return -1;
 }*/
int Alignment::getAlignmentFlag() {
	return al->AlignmentFlag;
}
string Alignment::getQueryBases() {
	return al->QueryBases;
}
string Alignment::getQualities() {
	return al->Qualities;
}
string convertInt(int number) {
	stringstream ss; //create a stringstream
	ss << number; //add number to the stream
	return ss.str(); //return a string with the contents of the stream
}
string Alignment::getTagData() {
	vector<string> tags;

	uint32_t i = 0;
	if (al->GetTag("AS", i)) {
		string tmp = "AS:i:";
		tmp += convertInt(i);
		tags.push_back(tmp);

	}
	i = 0;
	if (al->GetTag("NM", i)) {
		string tmp = "NM:i:";
		tmp += convertInt(i);
		tags.push_back(tmp);
	}

	string md;
	if (al->GetTag("MD", md)) {
		string tmp = "MD:Z:";
		tmp += md;
		tags.push_back(tmp);
	}

	i = 0;
	if (al->GetTag("UQ", i)) {
		string tmp = "UQ:i:";
		tmp += convertInt(i);
		tags.push_back(tmp);
	}
	string sa;
	if (al->GetTag("SA", sa)) {
		string tmp = "SA:Z:";
		tmp += sa;
		tags.push_back(tmp);
	}

	string res;
	for (size_t i = 0; i < tags.size(); i++) {
		res += tags[i];
		if (i + 1 < tags.size()) {
			res += '\t';
		}
	}
	return res;
}
void Alignment::initSequence() {
	this->alignment.first.clear();
	this->alignment.second.clear();
}

int Alignment::get_id(RefVector ref, std::string chr) {
	for (size_t i = 0; i < ref.size(); i++) {
		if (strcmp(ref[i].RefName.c_str(), chr.c_str()) == 0) {
			return i;
		}
	}
	return -1; //should not happen!
}

int get_readlen(std::vector<CigarOp> cigar) {
	int pos = 0;
	for (size_t i = 0; i < cigar.size(); i++) {
		if (cigar[i].Type == 'I') {
			pos += cigar[i].Length;
		} else if (cigar[i].Type == 'D') {
			//pos += cigar[i].Length;
		} else if (cigar[i].Type == 'M') {
			pos += cigar[i].Length;
		}
	}
	return pos;
}
void Alignment::get_coords(aln_str tmp, int & start, int &stop) {

	size_t index = 0;
	if (!tmp.strand) {
		index = tmp.cigar.size() - 1;
	}
	if (tmp.cigar[index].Type == 'S' || tmp.cigar[index].Type == 'H') {
		start = tmp.cigar[index].Length;
	} else {
		start = 0;
	}
	stop = get_readlen(tmp.cigar) + start;
}

void Alignment::check_entries(vector<aln_str> &entries) {
//Parameter::Instance()->read_name="0097b24b-1052-4c76-8a41-b66980e076f7_Basecall_Alignment_template";
	bool flag = (strcmp(this->getName().c_str(), Parameter::Instance()->read_name.c_str()) == 0);
//Given that we start outside of the INV:
	if (flag) {
		cout << "Check:" << endl;
		for (size_t i = 0; i < entries.size(); i++) {
			cout << "ENT: " << entries[i].pos << " " << entries[i].pos + entries[i].length << " Read: " << entries[i].read_pos_start << " " << entries[i].read_pos_stop << " ";
			if (entries[i].strand) {
				cout << "+" << endl;
			} else {
				cout << "-" << endl;
			}
		}
	}
	bool left_of = true;
	vector<aln_str> new_entries = entries;
	for (size_t i = 1; i < entries.size(); i++) {
		if (entries[i].strand != entries[i - 1].strand && entries[i].RefID == entries[i - 1].RefID) {
			int ref_dist = 0;
			int read_dist = 0;
			if (entries[0].strand) {
				ref_dist = abs((entries[i - 1].pos + entries[i - 1].length) - entries[i].pos);
				read_dist = abs(entries[i - 1].read_pos_stop - entries[i].read_pos_start);
			} else {
				ref_dist = abs((entries[i - 1].pos) - (entries[i].pos + entries[i].length));
				read_dist = abs(entries[i - 1].read_pos_stop - entries[i].read_pos_start);

			}
			if (flag) {
				cout << "ref dist: " << ref_dist << " Read: " << read_dist<<" DIFF: "<<abs(ref_dist - read_dist) << endl;
			}
			if (abs(ref_dist - read_dist) > Parameter::Instance()->min_length) {
				//ref_dist > 30 &&
				if (read_dist < Parameter::Instance()->min_length && ref_dist / (read_dist + 1) > 3) { //+1 because otherwise there could be a division by 0!
					if (flag) {
						cout << "DEL? " << ref_dist << " " << read_dist << endl;
					}
					aln_str tmp;
					tmp.RefID = entries[i].RefID;
					if (left_of) {
						tmp.strand = entries[i - 1].strand;
						if (tmp.strand) {
							tmp.pos = entries[i].pos - 1;
						} else {
							tmp.pos = entries[i].pos + entries[i].length - 1;
						}
						left_of = false;
					} else {
						tmp.strand = entries[i].strand;
						if (tmp.strand) {
							tmp.pos = entries[i - 1].pos + entries[i - 1].length - 1;
						} else {
							tmp.pos = entries[i - 1].pos - 1;
						}
						left_of = true;
					}
					tmp.length = 1;
					tmp.read_pos_start = entries[i].read_pos_start - 1;
					tmp.read_pos_stop = tmp.read_pos_start + 1;
					tmp.mq = 60;
					sort_insert(tmp, new_entries);
				} else if (read_dist > Parameter::Instance()->min_length && ref_dist < 10) {
					//cout << "INS? " << this->getName() << endl;
				}
			}
		} else {
			left_of = true; //??
		}
	}
	if (entries.size() < new_entries.size()) {
		entries = new_entries;
	}
}
void Alignment::sort_insert(aln_str tmp, vector<aln_str> &entries) {
	//TODO detect abnormal distances + directions -> introduce a pseudo base to detect these things later?

	for (vector<aln_str>::iterator i = entries.begin(); i != entries.end(); i++) {
		if ((tmp.read_pos_start == (*i).read_pos_start) && (tmp.pos == (*i).pos) && (tmp.strand == (*i).strand)) { //is the same! should not happen
			return;
		}
		if (!tmp.cigar.empty() && ((*i).read_pos_start <= tmp.read_pos_start && (*i).read_pos_stop >= tmp.read_pos_stop)) { //check for the additional introducded entries
			return;
		}
		if (!tmp.cigar.empty() && ((*i).read_pos_start >= tmp.read_pos_start && (*i).read_pos_stop <= tmp.read_pos_stop)) { //check for the additional introducded entries
			(*i) = tmp;
			return;
		}
		if ((tmp.read_pos_start < (*i).read_pos_start)) { //insert before
			entries.insert(i, tmp);
			return;
		}
	}
	entries.push_back(tmp);
}
vector<aln_str> Alignment::getSA(RefVector ref) {
	string sa;
	vector<aln_str> entries;
	if (al->GetTag("SA", sa) && !sa.empty()) {
		//store the main aln:
		aln_str tmp;
		tmp.RefID = this->getRefID();
		tmp.cigar = this->getCigar();
		tmp.length = (long) get_length(tmp.cigar);
		tmp.mq = this->getMappingQual();
		tmp.pos = (long) this->getPosition(); //+get_ref_lengths(tmp.RefID, ref);
		tmp.strand = getStrand();
		bool flag = strcmp(getName().c_str(), Parameter::Instance()->read_name.c_str()) == 0;

		get_coords(tmp, tmp.read_pos_start, tmp.read_pos_stop);
		entries.push_back(tmp);
		if (flag) {
			std::cout << "Main Read: read start:" << tmp.read_pos_start << " REF: " << tmp.pos << " RefID: " << tmp.RefID << std::endl;
		}
		size_t i = 0;
		int count = 0;

		std::string cigar;
		std::string chr;
		bool nested = true;
		while (i < sa.size()) {
			if (count == 0 && sa[i] != ',') {
				chr += sa[i];
			}
			if (count == 1 && sa[i - 1] == ',') {
				tmp.pos = (long) atoi(&sa[i]);
			}
			if (count == 2 && sa[i - 1] == ',') {
				tmp.strand = (bool) (sa[i] == '+');
			}
			if (count == 3 && sa[i] != ',') {
				cigar += sa[i];
			}
			if (count == 4 && sa[i - 1] == ',') {
				tmp.mq = atoi(&sa[i]);
			}
			if (count == 5 && sa[i] != ';') {
				tmp.nm = atoi(&sa[i]);
			}

			if (sa[i] == ',') {
				count++;
			}
			if (sa[i] == ';') {
				if (tmp.mq > Parameter::Instance()->min_mq && entries.size() <= Parameter::Instance()->max_splits) {
					//TODO: check this!
					tmp.cigar = translate_cigar(cigar); //translates the cigar (string) to a type vector
					get_coords(tmp, tmp.read_pos_start, tmp.read_pos_stop); //get the coords on the read.
					tmp.length = (long) get_length(tmp.cigar); //gives the length on the reference.
					tmp.RefID = get_id(ref, chr); //translates back the chr to the id of the chr;
					//TODO: should we do something about the MD string?
					if (flag) {
						std::cout << "Read: " << tmp.read_pos_start << " " << tmp.read_pos_stop << " REF: " << tmp.pos << " " << tmp.RefID;
						if (tmp.strand) {
							std::cout << "+" << std::endl;
						} else {
							std::cout << "-" << std::endl;
						}
					}
					//tmp.pos+=get_ref_lengths(tmp.RefID, ref);
					//insert sorted:
					includes_SV = true;
					sort_insert(tmp, entries);
				} else if (tmp.mq < Parameter::Instance()->min_mq) {
					nested = false;
				} else {					//Ignore read due to too many splits
					entries.clear();
					return entries;
				}
				chr.clear();
				cigar.clear();
				tmp.cigar.clear();
				count = 0;
				tmp.mq = 0;
			}
			i++;
		}
		if (nested && entries.size() > 2) {
			check_entries(entries);
		}
		if (flag) {
			for (size_t i = 0; i < entries.size(); i++) {
				cout << "ENT: " << entries[i].pos << " " << entries[i].pos + entries[i].length << " Read: " << entries[i].read_pos_start << " " << entries[i].read_pos_stop << " ";
				if (entries[i].strand) {
					cout << "+" << endl;
				} else {
					cout << "-" << endl;
				}
			}
		}
	}
	return entries;
}

//returns -1 if flags are not set!
double Alignment::get_scrore_ratio() {
	uint score = -1;
	uint subscore = -1;
	if (al->GetTag("AS", score) && al->GetTag("XS", subscore)) {
		if (subscore == 0) {
			subscore = 1;
		}
		//cout<<'\t'<<score<<" "<<subscore<<endl;
		return (double) score / (double) subscore;
	}
	return -1;
}
bool Alignment::get_is_save() {
	string sa;

	double score = get_scrore_ratio();

	/*if((al->GetTag("XA", sa) && !sa.empty())){
	 std::cout<<this->getName()<<"XA"<<std::endl;
	 }
	 if( (al->GetTag("XT", sa) && !sa.empty()) ){
	 std::cout<<this->getName()<<"XT"<<std::endl;
	 }*/

	return !((al->GetTag("XA", sa) && !sa.empty()) || (al->GetTag("XT", sa) && !sa.empty()));					//|| (score == -1 || score < Parameter::Instance()->score_treshold)); //TODO: 7.5
}

std::vector<CigarOp> Alignment::translate_cigar(std::string cigar) {

	std::vector<CigarOp> new_cigar;

	size_t i = 0;
	bool first = true;
	CigarOp tmp;
	tmp.Length = -1;
	while (i < cigar.size()) {
		if (tmp.Length == -1) {
			tmp.Length = atoi(&cigar[i]);
		} else if (tmp.Length != -1 && atoi(&cigar[i]) == 0 && cigar[i] != '0') {
			tmp.Type = cigar[i];
			new_cigar.push_back(tmp);

			tmp.Length = -1;
			first = false;
		}
		i++;
	}
	return new_cigar;
}

double Alignment::get_avg_indel_length_Cigar() {
	double len = 0;
	double num = 0;
	for (size_t i = 0; i < al->CigarData.size(); i++) {
		if ((al->CigarData[i].Type == 'I' || al->CigarData[i].Type == 'D') && al->CigarData[i].Length > 1) {
			len += al->CigarData[i].Length;
			num++;
		}
	}

	return len / num;
}

vector<str_event> Alignment::get_events_CIGAR() {

	size_t read_pos = 0;
	size_t pos = this->getPosition(); //orig_length;
	vector<str_event> events;
	for (size_t i = 0; i < al->CigarData.size(); i++) {
		if (al->CigarData[i].Type == 'H' || (al->CigarData[i].Type == 'S' || al->CigarData[i].Type == 'M')) {
			read_pos += al->CigarData[i].Length;
		}
		if (al->CigarData[i].Type == 'D' && al->CigarData[i].Length > Parameter::Instance()->min_cigar_event) {
			str_event ev;
			ev.read_pos = read_pos;
			ev.length = al->CigarData[i].Length; //deletion
			ev.pos = pos;
			includes_SV = true;
			events.push_back(ev);
		}
		if (al->CigarData[i].Type == 'I' && al->CigarData[i].Length > Parameter::Instance()->min_cigar_event) {
			//	std::cout<<"CIGAR: "<<al->CigarData[i].Length<<" "<<this->getName()<<std::endl;
			str_event ev;
			ev.length = al->CigarData[i].Length * -1; //insertion;
			ev.pos = pos;
			ev.read_pos = read_pos;
			includes_SV = true;
			events.push_back(ev);
			read_pos += al->CigarData[i].Length;
		}
		if (al->CigarData[i].Type == 'D' || al->CigarData[i].Type == 'M' || al->CigarData[i].Type == 'N') {
			pos += al->CigarData[i].Length;
		}
	}

	return events;
}

double Alignment::get_num_mismatches(std::string md) {
	bool deletion = false;
	bool match = false;
	vector<int> helper;
	double mis = 0;
	double len = 0;
	double maxim = 0;
	for (size_t i = 0; i < md.size(); i += 20) {
		mis = 0;
		len = 0;
		for (size_t j = 0; len < 100 && j + i < md.size(); j++) {
			if (match && atoi(&md[i + j]) == 0 && md[i + j] != '0') { //is not a number:
				if (md[i] == '^') {
					deletion = true;
				} else {
					len++;
				}
				if (!deletion) {
					//mistmatch!!
					mis++;
					match = false;
				}
			} else {
				len += atoi(&md[i + j]);

				match = true;
				deletion = false;
			}
		}

		if (strcmp(getName().c_str(), Parameter::Instance()->read_name.c_str()) == 0) {
			std::cout << (mis / len) << std::endl;
		}
		if ((mis / len) > maxim) {
			maxim = (mis / len);
		}
	}
	return maxim; // 0.03);
}
std::string Alignment::get_md() {
	std::string md;
	if (al->GetTag("MD", md)) {
		return md;
	}
	return md;
}
vector<str_event> Alignment::get_events_MD(int min_mis) {
	vector<str_event> events;
	/*std::string md;
	 if (al->GetTag("MD", md)) {
	 //TODO: remove:
	 bool flag = strcmp(getName().c_str(), Parameter::Instance()->read_name.c_str()) == 0;

	 if (flag) {
	 std::cout << "found!" << std::endl;
	 }
	 //TODO think of a good threshold!
	 if (get_num_mismatches(md) > Parameter::Instance()->min_num_mismatches) {
	 if (flag) {
	 std::cout << "is_strange!" << std::endl;
	 }
	 //generate a vector that holds the positions of the read
	 std::vector<int> aln;
	 int pos = getPosition();

	 for (size_t i = 0; i < al->CigarData.size(); i++) {
	 if (al->CigarData[i].Type == 'I') { //TODO check
	 }
	 if (al->CigarData[i].Type == 'D') {
	 pos += al->CigarData[i].Length;
	 }
	 if (al->CigarData[i].Type == 'M') {
	 for (size_t j = 0; j < al->CigarData[i].Length; j++) {
	 aln.push_back(pos);
	 pos++;
	 //aln += "=";
	 }
	 }
	 }
	 //fill in the mismatches:
	 bool deletion = false;
	 bool match = false;
	 double mis = 0;
	 double len = 0;
	 for (size_t i = 0; i < md.size(); i++) {
	 if ((atoi(&md[i]) == 0 && md[i] != '0')) { //is not a number:
	 if (md[i] == '^') {
	 deletion = true;
	 }
	 if (!deletion) {
	 //mistmatch!!
	 mis++;
	 aln[len] = aln[len] * -1;
	 len++;
	 }
	 match = false;
	 } else if (!match) {
	 len += atoi(&md[i]);
	 match = true;
	 deletion = false;
	 }
	 }


	 int runlength = 100;
	 str_event ev;
	 ev.pos = -1;
	 ev.length = -1;
	 ev.read_pos = 0;
	 int start = 0;
	 int last = 0;
	 for (size_t i = 0; i < aln.size(); i += 50) { //+=runlength/2 ??
	 //std::cout<<aln[i]<<";";
	 int mis = 0;
	 int first = 0;

	 for (size_t j = 0; (j + i) < aln.size() && j < runlength; j++) {
	 if (aln[(i + j)] < 0) {
	 if (first == 0) {
	 first = abs(aln[(i + j)]);
	 }
	 last = abs(aln[(i + j)]);
	 mis++;
	 }
	 }
	 if (mis > min_mis) { //TOOD ratio?
	 if (ev.pos == -1) {
	 start = i;
	 ev.pos = first;
	 ev.read_pos = ev.pos - getPosition();
	 }
	 } else {
	 if ((start > 20 && abs((int) (i + runlength) - (int) aln.size()) > 20) && ev.pos != -1) {
	 if (flag) {
	 std::cout << i << " " << (i + runlength) << " " << aln.size() << std::endl;
	 std::cout << ev.pos << " " << last << " " << std::endl;
	 }
	 includes_SV = true;
	 ev.length = last - ev.pos;
	 if (flag) {
	 std::cout << ev.pos << " " << ev.length << std::endl;
	 }
	 if (ev.length > runlength) {
	 events.push_back(ev);
	 }
	 last = 0;
	 ev.pos = -1;
	 } else {
	 ev.pos = -1;
	 }
	 }
	 }
	 }

	 }*/
	return events;
}

vector<int> Alignment::get_avg_diff(double & dist) {

	//computeAlignment();
	//cout<<alignment.first<<endl;
	//cout<<alignment.second<<endl;

	vector<differences_str> event_aln = summarizeAlignment();
	vector<int> mis_per_window;
	PlaneSweep_slim * plane = new PlaneSweep_slim();
	int min_tresh = 5; //reflects a 10% error rate.
	//compute the profile of differences:
	for (size_t i = 0; i < event_aln.size(); i++) {
		if (i != 0) {
			dist += event_aln[i].position - event_aln[i - 1].position;
		}
		pair_str tmp;
		tmp.position = -1;
		if (event_aln[i].type == 0) {
			tmp = plane->add_mut(event_aln[i].position, 1, min_tresh);
		} else {
			tmp = plane->add_mut(event_aln[i].position, abs(event_aln[i].type), min_tresh);
		}
		if (tmp.position != -1) { //check that its not the prev event!
			mis_per_window.push_back(tmp.coverage); //store #mismatch per window each time it exceeds. (which might be every event position!)
		}
	}
	dist = dist / (double) event_aln.size();
	plane->finalyze();
	return mis_per_window;	//total_num /num;
}

vector<str_event> Alignment::get_events_Aln() {

	//clock_t comp_aln = clock();
	vector<differences_str> event_aln = summarizeAlignment();
	//double time2 = Parameter::Instance()->meassure_time(comp_aln, "\tcompAln Events: ");

	vector<str_event> events;
	PlaneSweep_slim * plane = new PlaneSweep_slim();
	vector<pair_str> profile;
//	comp_aln = clock();
	//Parameter::Instance()->read_name = "21_30705246";
	bool flag = (strcmp(this->getName().c_str(), Parameter::Instance()->read_name.c_str()) == 0);
	//cout<<" IDENT: "<<(double)event_aln.size()/(double)this->al->Length << " "<<this->getName().c_str()<<endl;

	//compute the profile of differences:
	for (size_t i = 0; i < event_aln.size(); i++) {
		pair_str tmp;
		tmp.position = -1;
		if (event_aln[i].type == 0) {
			tmp = plane->add_mut(event_aln[i].position, 1, Parameter::Instance()->window_thresh);
		} else {
			tmp = plane->add_mut(event_aln[i].position, 1, Parameter::Instance()->window_thresh);	// abs(event_aln[i].type)
		}
		if (tmp.position != -1 && (profile.empty() || (tmp.position - profile[profile.size() - 1].position) > 100)) {	//for noisy events;
			profile.push_back(tmp);
			if (flag) {
				cout << "HIT: " << event_aln[i].position << " " << tmp.coverage << endl;
			}
		} else if (abs(event_aln[i].type) > Parameter::Instance()->min_length) {	//for single events like NGM-LR would produce them.
			tmp.position = event_aln[i].position;
			profile.push_back(tmp);
		}
	}
	//Parameter::Instance()->meassure_time(comp_aln, "\tcompProfile: ");
	/*if (strcmp(getName().c_str(), Parameter::Instance()->read_name.c_str()) == 0) {
	 int prev = 0;
	 prev = getPosition();
	 for (size_t i = 0; i < event_aln.size(); i++) {
	 cout << event_aln[i].position - prev << " " << event_aln[i].type << endl;

	 }
	 }*/

	//comp_aln = clock();
	size_t stop = 0;
	size_t start = 0;
	int tot_len = 0;
	for (size_t i = 0; i < profile.size(); i++) {
		if (profile[i].position > event_aln[stop].position) {
			//find the postion:
			size_t pos = 0;
			while (pos < event_aln.size() && event_aln[pos].position != profile[i].position) {
				pos++;
			}
			//run back to find the start:
			start = pos;
			int prev = event_aln[pos].position;
			start = pos;
			int prev_type = 1;
			//todo it is actually pos + type and not *type
			while (start > 0 && (prev - event_aln[start].position) < (Parameter::Instance()->max_dist_alns)) {	//13		//} * abs(event_aln[start].type) + 1)) { //TODO I  dont like 13!??
				prev = event_aln[start].position;
				prev_type = abs(event_aln[start].type);
				start--;

				if (prev_type == 0) {
					prev_type = 1;
				}
				prev += prev_type;
			}
			start++; //we are running one too far!

			//run forward to identify the stop:
			prev = event_aln[pos].position;
			stop = pos;
			prev_type = 1;

			while (stop < event_aln.size() && (event_aln[stop].position - prev) < (Parameter::Instance()->max_dist_alns)) {		// * abs(event_aln[stop].type) + 1)) {
				prev = event_aln[stop].position;

				prev_type = abs(event_aln[stop].type);
				stop++;
				if (prev_type == 0) {
					prev_type = 1;
				}
				prev += prev_type;
			}
			stop--;

			int insert_max_pos = 0;
			int insert_max = 0;
			if (event_aln[start].type < 0) {
				insert_max_pos = event_aln[start].position;
				insert_max = abs(event_aln[start].type);
			}

			int del_max = 0;
			int del_max_pos = 0;

			double insert = 0;
			double del = 0;
			double mismatch = 0;

			for (size_t k = start; k < stop; k++) {
				if (flag) {
					//		cout << event_aln[k].position << " " << event_aln[k].type << endl;
				}
				if (event_aln[k].type == 0) {
					mismatch++;
				} else if (event_aln[k].type > 0) {
					del += abs(event_aln[k].type);
					if (del_max < abs(event_aln[k].type)) {
						del_max = abs(event_aln[k].type);
						del_max_pos = event_aln[k].position;
					}
				} else if (event_aln[k].type < 0) {
					insert += abs(event_aln[k].type);
					if (insert_max < abs(event_aln[k].type)) {
						insert_max = abs(event_aln[k].type);
						insert_max_pos = event_aln[k].position;
					}
				}
			}
			str_event tmp;
			tmp.pos = event_aln[start].position;

			tmp.length = event_aln[stop].position;
			if (event_aln[stop].type > 1) {		//because of the way we summarize mutations to one location
				tmp.length += event_aln[stop].type;
			}
			tmp.length = (tmp.length - event_aln[start].position);

			tmp.type = 0;
			//TODO constance used!
			if (insert_max > Parameter::Instance()->min_length && insert > (del + del)) { //we have an insertion! //todo check || vs. &&
				if (flag) {
					cout << "store INS" << endl;
				}
				tmp.length = insert_max; //TODO not sure!
				tmp.pos = insert_max_pos;
				tmp.type |= INS;
			} else if (del_max > Parameter::Instance()->min_length && (mismatch < 2 && (insert + insert) < del)) { //deletion
				if (flag) {
					cout << "store DEL" << endl;
				}
				tmp.type |= DEL;
			} else if (mismatch > Parameter::Instance()->min_length) { //TODO
				if (flag) {
					cout << "store Noise" << endl;
				}
				tmp.type |= DEL;
				tmp.type |= INV;
			}else{
			//	std::cout<<"ERROR we did not set the type!"<<std::endl;
			}

			if (flag) {
				cout << "Read: " << " " << (double) this->getRefLength() << " events: " << event_aln.size() << " " << this->al->Name << std::endl;
				cout << "INS max " << insert_max << " del_max " << del_max << std::endl;
				cout << "INS:" << insert << " DEL: " << del << " MIS: " << mismatch << endl;
				cout << event_aln[start].position << " " << event_aln[stop].position << endl;
				cout << tot_len << " " << this->getRefLength() << endl;
				cout << "store: " << tmp.pos << " " << tmp.pos + abs(tmp.length) << " " << tmp.length << endl;
				cout << endl;
			}

			tot_len += tmp.length;

			if (tot_len > this->getRefLength() * 0.77) { // if the read is just noisy as hell:
				events.clear();
				return events;
			} else if(tmp.type!=0) {
				if (flag) {
					cout << "STORE" << endl;
				}
				events.push_back(tmp);
			}
		}
	}
//	Parameter::Instance()->meassure_time(comp_aln, "\tcompPosition: ");
	if (events.size() > 4) { //TODO very arbitrary! test?
		events.clear();
	}
	return events;
}


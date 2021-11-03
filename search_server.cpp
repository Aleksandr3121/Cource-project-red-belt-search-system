#include "search_server.h"
#include "iterator_range.h"

#include <algorithm>
#include <iterator>
#include <sstream>
#include <iostream>
#include <cmath>

using namespace std::chrono;

vector<string> SplitIntoWords(const string& line) {
    vector<string> result;
    string_view str = line;
    size_t pos = 0;
    while (pos != string::npos) {
        pos = str.find_first_not_of(' ');
        if (pos == string::npos)return result;
        str.remove_prefix(pos);
        pos = str.find(' ');
        result.push_back(string(str.substr(0, pos)));
        if (pos != string::npos) str.remove_prefix(pos);
    }
    return result;
}

vector<string_view> SplitIntoWordsView(const string& line) {
    vector<string_view> result;
    string_view str = line;
    size_t pos = 0;
    while (pos != string::npos) {
        pos = str.find_first_not_of(' ');
        if (pos == string::npos)return result;
        str.remove_prefix(pos);
        pos = str.find(' ');
        result.push_back(str.substr(0, pos));
        if (pos != string::npos) str.remove_prefix(pos);
    }
    return result;
}



SearchServer::SearchServer(istream& document_input) {
  UpdateDocumentBaseOneThread(document_input);
}

void SearchServer::UpdateDocumentBaseOneThread(istream& document_input) {
  InvertedIndex new_index;

  for (string current_document; getline(document_input, current_document); ) {
    new_index.Add(move(current_document));
  }
  m.lock();
  index = move(new_index);
  m.unlock();
}

void SearchServer::AddQueriesStreamOneThread(istream& query_input, ostream& search_results_output) {
    string current_query;
    
    while(getline(query_input, current_query)) {
        const auto words = SplitIntoWordsView(current_query);
        while (index.Size()==0) {
            this_thread::sleep_for(50ms);
        }
        m.lock();
        vector<size_t> docid_count(index.Size(), 0);
        for (const auto& word : words) {
            for (const auto& p : index.Lookup(word)) {
                docid_count[p.first] += p.second;
            }
        }
        m.unlock();
        vector<pair<size_t, size_t>> search_results;
        for (int i = 0; i < docid_count.size(); ++i) {
            if (docid_count[i] == 0)continue;
            if (search_results.size() < 5) {
                search_results.push_back({ i, docid_count[i] });
                if (search_results.size() == 5) {
                    sort(
                        search_results.begin(),
                        search_results.end(),
                        [](pair<size_t, size_t>& lhs, pair<size_t, size_t>& rhs) {
                            if (lhs.second != rhs.second)return lhs.second > rhs.second;
                            return lhs.first < rhs.first;
                        }
                        );
                }
            }
            else {
                if (docid_count[i] <= search_results[4].second)continue;
                search_results[4].first = i;
                search_results[4].second = docid_count[i];
                for (int i = 4; i > 0; --i) {
                    if (search_results[i].second > search_results[i - 1].second) {
                        swap(search_results[i], search_results[i - 1]);
                    }
                    else break;
                }
            }
        }
        if (search_results.size() < 5) {
            sort(
                search_results.begin(),
                search_results.end(),
                [](pair<size_t, size_t>& lhs, pair<size_t, size_t>& rhs) {
                    if (lhs.second != rhs.second)return lhs.second > rhs.second;
                    return lhs.first < rhs.first;
                }
            );
        }
        search_results_output << current_query << ':';
        for (const auto& p : search_results) {
            search_results_output << " {"
                << "docid: " << p.first << ", "
                << "hitcount: " << p.second << '}';
        }
        search_results_output << endl;
    }
}



void InvertedIndex::Add(string document) {
    docs.push_back(move(document));

    const size_t docid = docs.size() - 1;
    for (auto& word : SplitIntoWordsView(docs.back())) {
        auto& ref = index[word];
        if (ref.empty() || ref.back().first != docid)ref.push_back({docid, 0});
        ref.back().second++;
    }
}

const vector<pair<size_t, size_t>>& InvertedIndex::Lookup(string_view word) const {
    auto it = index.find(word);
    if (it != index.end()) {
        return it->second;
    } else {
        return emptyVector;
    }
}

void SearchServer::AddQueriesStream(istream& query_input, ostream& search_results_output) {
    futures.push_back(async(
        &SearchServer::AddQueriesStreamOneThread,
        this, ref(query_input), ref(search_results_output)
    ));
}

void SearchServer::UpdateDocumentBase(istream& document_input) {
    futures.push_back(async(
        &SearchServer::UpdateDocumentBaseOneThread,
        this, ref(document_input)
    ));
}

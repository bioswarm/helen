//
// Created by Kishwar Shafin on 11/1/18.
//

#include "../../headers/pileup_summary/summary_generator.h"

SummaryGenerator::SummaryGenerator(string reference_sequence, string chromosome_name, long long ref_start,
                                   long long ref_end) {
    this->reference_sequence = reference_sequence;
    this->ref_start = ref_start;
    this->ref_end = ref_end;
}


int get_mapping_quality_bin(int mapping_quality) {
    if (mapping_quality >= 40) return 0;
    if (mapping_quality >= 10 && mapping_quality <= 20) return 1;
    if (mapping_quality >= 5 && mapping_quality < 10) return 2;
    return 3;
}

int get_base_bin(char base) {
    int quality_band = 4;
    int base_start_index = 1; //index 0 is to indiciate if it's an insert
    if (base == 'A') return base_start_index;
    if (base == 'C') return base_start_index + 1 * quality_band;
    if (base == 'G') return base_start_index + 2 * quality_band;
    if (base == 'T') return base_start_index + 3 * quality_band;
    return base_start_index + 4 * quality_band;
}

int get_labels(char base) {
    if (base == 'A' or base == 'a') return 1;
    if (base == 'C' or base == 'c') return 2;
    if (base == 'T' or base == 't') return 3;
    if (base == 'G' or base == 'g') return 4;
    return 0;
}

int get_index(char base, int mapping_quality) {
    return get_base_bin(base) + get_mapping_quality_bin(mapping_quality);
}

double get_base_quality_weight(int base_quality) {
    double p = 1.0 - pow(10, (-1.0 * (double) base_quality) / 10.0);
    return p;
}


void SummaryGenerator::iterate_over_read(type_read read, long long region_start, long long region_end) {
    int read_index = 0;
    long long ref_position = read.pos;
    int cigar_index = 0;
    int base_quality = 0;
    long long reference_index;

    for (auto &cigar: read.cigar_tuples) {
        if (ref_position > region_end) break;
        switch (cigar.operation) {
            case CIGAR_OPERATIONS::EQUAL:
            case CIGAR_OPERATIONS::DIFF:
            case CIGAR_OPERATIONS::MATCH:
                cigar_index = 0;
                if (ref_position < ref_start) {
                    cigar_index = min(ref_start - ref_position, (long long) cigar.length);
                    read_index += cigar_index;
                    ref_position += cigar_index;
                }
                for (int i = cigar_index; i < cigar.length; i++) {
                    reference_index = ref_position - ref_start;
                    //read.base_qualities[read_index] base quality
                    if (ref_position >= ref_start && ref_position <= ref_end) {
                        char base = read.sequence[read_index];

                        // update the summary of base
                        base_summaries[make_pair(ref_position, get_index(base, read.mapping_quality))] +=
                                get_base_quality_weight(read.base_qualities[read_index]);
                        coverage[ref_position] += 1;
                    }
                    read_index += 1;
                    ref_position += 1;
                }
                break;
            case CIGAR_OPERATIONS::IN:
//                base_qualities = read.base_qualities.begin() + read_index, read.base_qualities.begin() + (read_index + cigar.length);
                reference_index = ref_position - ref_start - 1;

                if (ref_position - 1 >= ref_start &&
                    ref_position - 1 <= ref_end) {
                    // process insert allele here
                    string alt;
                    alt = read.sequence.substr(read_index, cigar.length);
                    for (int i = 0; i < cigar.length; i++) {
                        pair<long long, int> position_pair = make_pair(ref_position - 1, i);
                        insert_summaries[make_pair(position_pair, get_index(alt[i], read.mapping_quality))] +=
                                get_base_quality_weight(read.base_qualities[read_index + i]);
                    }
                    longest_insert_count[ref_position - 1] = std::max(longest_insert_count[ref_position - 1],
                                                                      (long long) alt.length());
                }
                read_index += cigar.length;
                break;
            case CIGAR_OPERATIONS::REF_SKIP:
            case CIGAR_OPERATIONS::PAD:
            case CIGAR_OPERATIONS::DEL:
//                base_quality = read.base_qualities[max(0, read_index)];
                reference_index = ref_position - ref_start - 1;

                if (ref_position >= ref_start && ref_position <= ref_end) {
                    // process delete allele here
                    for (int i = 0; i < cigar.length; i++) {
                        if (ref_position + i >= ref_start && ref_position + i <= ref_end) {
                            // update the summary of base
                            base_summaries[make_pair(ref_position + i, get_index('*', read.mapping_quality))] += 0.75;
                        }
                    }
//                    cout<<"DEL: "<<ref_position<<" "<<ref<<" "<<alt<<" "<<endl;
                }
                ref_position += cigar.length;
                break;
            case CIGAR_OPERATIONS::SOFT_CLIP:
                read_index += cigar.length;
                break;
            case CIGAR_OPERATIONS::HARD_CLIP:
                break;
        }
    }
//    cout<<endl;
}

int SummaryGenerator::get_sequence_length(long long start_pos, long long end_pos) {
    int length = 0;
    for (int i = start_pos; i <= end_pos; i++) {
        length += 1;
        if (longest_insert_count[i] > 0) {
            length += longest_insert_count[i];
        }
    }
    return length;
}

//bool check_base(char base) {
//    if(base=='A' || base=='a' || base=='C' || base=='c' || base=='T' || base=='t' || base =='G' || base==;)
//}

void SummaryGenerator::generate_labels(type_read read, long long region_start, long long region_end) {
    int read_index = 0;
    long long ref_position = read.pos;
    int cigar_index = 0;
    int base_quality = 0;
    long long reference_index;

    for (auto &cigar: read.cigar_tuples) {
        if (ref_position > region_end) break;
        switch (cigar.operation) {
            case CIGAR_OPERATIONS::EQUAL:
            case CIGAR_OPERATIONS::DIFF:
            case CIGAR_OPERATIONS::MATCH:
                cigar_index = 0;
                if (ref_position < ref_start) {
                    cigar_index = min(ref_start - ref_position, (long long) cigar.length);
                    read_index += cigar_index;
                    ref_position += cigar_index;
                }
                for (int i = cigar_index; i < cigar.length; i++) {
                    reference_index = ref_position - ref_start;
                    //read.base_qualities[read_index] base quality
                    if (ref_position >= ref_start && ref_position <= ref_end) {
                        char base = read.sequence[read_index];

//                            double count = 0.0;
//                            for(int j=0;j<4;j++)
//                              count += base_summaries[make_pair(ref_position, get_base_bin(base) + j)];
//
//                            if(count == 0) {
//                                cerr<<"BASE LABEL HAS NO FREQUENCY: ";
//                                cerr<<chromosome_name<<" "<<ref_position<<" true label:"<<base<<" "<<count<<endl;
//                            }
                        base_labels[ref_position] = base;
                    }
                    read_index += 1;
                    ref_position += 1;
                }
                break;
            case CIGAR_OPERATIONS::IN:
//                base_qualities = read.base_qualities.begin() + read_index, read.base_qualities.begin() + (read_index + cigar.length);
                reference_index = ref_position - ref_start - 1;
                if (ref_position - 1 >= ref_start &&
                    ref_position - 1 <= ref_end) {
                    // process insert allele here
                    string alt;
                    alt = read.sequence.substr(read_index, cigar.length);

//                        if(longest_insert_count[ref_position-1] < cigar.length) {
//                            cerr<<"INSERT LENGTH TO TRUTH LENGTH DON'T MATCH: ";
//                            cerr<<chromosome_name<<" "<<ref_position<<" true label:"<<alt<<endl;
//                        } else {

                    for (int i = 0; i < longest_insert_count[ref_position - 1]; i++) {
                        char base = 'T';
                        if (i < alt.length()) {
                            base = alt[i];
                        }
                        pair<long long, int> position_pair = make_pair(ref_position - 1, i);
                        insert_labels[position_pair] = base;
                    }
//                        }
                    // INSERT
                }
                read_index += cigar.length;
                break;
            case CIGAR_OPERATIONS::REF_SKIP:
            case CIGAR_OPERATIONS::PAD:
            case CIGAR_OPERATIONS::DEL:
//                base_quality = read.base_qualities[max(0, read_index)];
                reference_index = ref_position - ref_start - 1;

                if (ref_position >= ref_start && ref_position <= ref_end) {
                    // process delete allele here
                    for (int i = 0; i < cigar.length; i++) {
                        if (ref_position + i >= ref_start && ref_position + i <= ref_end) {
                            // DELETE
                            char base = '*';
//                                double count = 0.0;
//                                for(int j=0;j<4;j++)
//                                    count += base_summaries[make_pair(ref_position, get_base_bin(base) + j)];
//
//                                if(count == 0) {
//                                    cerr<<"BASE-DELETE LABEL HAS NO FREQUENCY: ";
//                                    cerr<<chromosome_name<<" "<<ref_position<<" true label:"<<base<<" "<<count<<endl;
//                                }
                            base_labels[ref_position + i] = '*';
                        }
                    }
                }
                ref_position += cigar.length;
                break;
            case CIGAR_OPERATIONS::SOFT_CLIP:
                read_index += cigar.length;
                break;
            case CIGAR_OPERATIONS::HARD_CLIP:
                break;
        }
    }
}


void SummaryGenerator::debug_print(long long start_pos, long long end_pos) {
    cout << setprecision(2);
    for (int i = start_pos; i <= end_pos; i++) {
        cout << reference_sequence[i - start_pos] << "\t";
        if (longest_insert_count[i] > 0) {
            for (int ii = 0; ii < longest_insert_count[i]; ii++)cout << "*" << "\t";
        }
    }
    cout << endl;
    for (int i = start_pos; i <= end_pos; i++) {
        cout << base_labels[i] << "\t";
        if (longest_insert_count[i] > 0) {
            for (int ii = 0; ii < longest_insert_count[i]; ii++) {
                if (insert_labels[make_pair(i, ii)]) cout << insert_labels[make_pair(i, ii)] << "\t";
                else cout << "*" << "\t";
            }
        }
    }
    cout << endl;
    for (int i = 0; i < labels.size(); i++) {
        cout << labels[i] << "\t";
    }
    cout << endl;
    for (int i = 0; i < labels.size(); i++) {
        cout << "(" << genomic_pos[i].first << "," << genomic_pos[i].second << ")\t";
    }
    cout << endl;

    // for each base
    for (int b = 0; b < 5; b++) {
        char base = 'A';
        if (b == 1)base = 'C';
        if (b == 2)base = 'G';
        if (b == 3)base = 'T';
        if (b == 4)base = '*';
        cout << base << endl;
        // for each quality bin
        for (int j = 0; j < 4; j++) {
            // go through all the positions
            for (int i = start_pos; i <= end_pos; i++) {
                cout << base_summaries[make_pair(i, get_base_bin(base) + j)] << "\t";

                if (longest_insert_count[i] > 0) {
                    for (int ii = 0; ii < longest_insert_count[i]; ii++) {
                        cout << insert_summaries[make_pair(make_pair(i, ii), get_base_bin(base) + j)] << "\t";
                    }
                }
            }
            cout << endl;
        }
    }

    cout << "-------------" << endl;
    for (int i = 0; i < image.size(); i++) {
        for (int j = 0; j < image[i].size(); j++) {
            printf("%.3lf\t", image[i][j]);
        }
        cout << endl;
    }

}


void SummaryGenerator::generate_summary(vector <type_read> &reads,
                                        long long start_pos,
                                        long long end_pos,
                                        type_read truth_read,
                                        bool train_mode) {
    for (auto &read:reads) {
        // this populates base_summaries and insert_summaries dictionaries
        iterate_over_read(read, start_pos, end_pos);
    }

    if (train_mode) {
        // this populates base_labels and insert_labels dictionaries
        generate_labels(truth_read, start_pos, end_pos);
    }

    // after all the dictionaries are populated, we can simply walk through the region and generate a sequence
    if (train_mode) {
        for (long long pos = start_pos; pos <= end_pos; pos++) {
            labels.push_back(get_labels(base_labels[pos]));
            genomic_pos.push_back(make_pair(pos, 0));
            if (longest_insert_count[pos] > 0) {
                for (int ii = 0; ii < longest_insert_count[pos]; ii++) {
                    genomic_pos.push_back(make_pair(pos, ii + 1));
                    if (insert_labels[make_pair(pos, ii)])
                        labels.push_back(get_labels(insert_labels[make_pair(pos, ii)]));
                    else labels.push_back(get_labels('*'));
                }
            }
        }
        assert(labels.size() == genomic_pos.size());
    } else {
        // otherwise only generate the positions
        for (long long pos = start_pos; pos <= end_pos; pos++) {
            int indx = 0;
            genomic_pos.push_back(make_pair(pos, 0));
            if (longest_insert_count[pos] > 0) {
                for (int ii = 0; ii < longest_insert_count[pos]; ii++)
                    genomic_pos.push_back(make_pair(pos, ii + 1));
            }
        }
    }
    // at this point labels and positions are generated, now generate the pileup
    for (long long i = start_pos; i <= end_pos; i++) {
        vector<double> row;
        for (int b = 0; b < 5; b++) {
            char base = 'A';
            if (b == 1)base = 'C';
            if (b == 2)base = 'G';
            if (b == 3)base = 'T';
            if (b == 4)base = '*';
            for (int j = 0; j < 4; j++) {
                double val = 0.0;
                if (coverage[i] > 0) val = base_summaries[make_pair(i, get_base_bin(base) + j)] / coverage[i];
                row.push_back(val);
            }
        }
        assert(row.size() == 20);
        image.push_back(row);

        if (longest_insert_count[i] > 0) {

            for (int ii = 0; ii < longest_insert_count[i]; ii++) {
                vector<double> ins_row;
                for (int b = 0; b < 5; b++) {
                    char base = 'A';
                    if (b == 1)base = 'C';
                    if (b == 2)base = 'G';
                    if (b == 3)base = 'T';
                    if (b == 4)base = '*';
                    for (int j = 0; j < 4; j++) {
                        double val = 0.0;
                        if (coverage[i] > 0)
                            val = insert_summaries[make_pair(make_pair(i, ii), get_base_bin(base) + j)] / coverage[i];
                        ins_row.push_back(val);
                    }
                }
                assert(ins_row.size() == 20);
                image.push_back(ins_row);
            }

        }

    }
    assert(image.size() == genomic_pos.size());
    // at this point everything should be generated
//    debug_print(start_pos, end_pos);
}
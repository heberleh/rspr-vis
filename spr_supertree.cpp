/*******************************************************************************
spr_supertree.cpp

Copyright 2011 Chris Whidden
whidden@cs.dal.ca
http://kiwi.cs.dal.ca/Software/RSPR
August 22, 2011

This file is part of rspr.

rspr is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

rspr is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with rspr.  If not, see <http://www.gnu.org/licenses/>.

TODO:

*******************************************************************************
ALGORITHM
*******************************************************************************

These options control what algorithm is used

-fpt        Calculate the exact rSPR distance with an FPT algorithm

-bb         Calculate the exact rSPR distance with a branch-and-bound
            FPT algorithm. This is the default option.

-approx		Calculate just a linear -time 3-approximation of the rSPR distance

*******************************************************************************
OPTIMIZATIONS
*******************************************************************************

These options control the use of optimized branching. All optimizations are
enabled by default. Specifying any subset of -cob, -cab, and -sc will use
just that subset of optimizations. See the README for more information.

-allopt		Use -cob -cab -sc. This is the default option

-noopt		Use 3-way branching for all FPT algorithms

-cob		Use "cut one b" improved branching

-cab		Use "cut all b" improved branching

-sc			Use "separate components" improved branching

*******************************************************************************
UNROOTED COMPARISON OPTIONS
*******************************************************************************

-unrooted   Compare the first input tree to each other input tree.
            Output the best found distance and agreement forest
-unrooted_min_approx    Compare the first input tree to each other input tree.
                        Run the exact algorithms on the pair with the
                        minimum approximate rspr distance

*******************************************************************************
OTHER OPTIONS
*******************************************************************************
-cc         Calculate a potentially better approximation with a quadratic time
            algorithm

-q          Quiet; Do not output the input trees or approximation
*******************************************************************************

Example:
$ ./rspr < test_trees/trees2.txt
T1: ((((1,2),(3,4)),((5,6),(7,8))),(((9,10),(11,12)),((13,14),(15,16))))
T2: ((((3,4),(8,(2,((11,12),1)))),((15,16),(7,(6,5)))),(14,((10,13),9)))

F1: ((3,4),(5,6)) 13 14 10 (11,12) 9 1 8 7 2 (15,16)
F2: ((3,4),(6,5)) 13 10 14 (11,12) 1 9 8 2 7 (15,16)
approx drSPR=12

4
F1: ((((1,2),(3,4)),((5,6),7)),((9,10),14)) 13 (11,12) 8 (15,16)
F2: ((((3,4),(2,1)),(7,(6,5))),(14,(10,9))) 13 (11,12) 8 (15,16)
exact BB drSPR=4

*******************************************************************************/

#include <cstdio>
#include <cstdlib>
#include <string>
#include <cstring>
#include <iostream>
#include <sstream>
#include <climits>
#include <vector>
#include <map>
#include <unordered_map>
#include <list>
#include "rspr.h"

#include "Forest.h"
#include "ClusterForest.h"
#include "LCA.h"
#include "ClusterInstance.h"
#include "UndoMachine.h"

using namespace std;


// options to pick default
bool DEFAULT_ALGORITHM=true;
bool DEFAULT_OPTIMIZATIONS=true;


bool FPT = false;
bool QUIET = false;
bool UNROOTED = false;
bool UNROOTED_MIN_APPROX = false;
bool LCA_TEST = false;
bool CLUSTER_TEST = false;

string USAGE =
"rspr, version 1.01\n"
"\n"
"usage: rspr [OPTIONS]\n"
"Calculate approximate and exact Subtree Prune and Regraft (rSPR)\n"
"distances and the associated maximum agreement forests (MAFs) between pairs\n"
"of rooted binary trees from STDIN in newick format. By default, computes a\n"
"3-approximation of the rSPR distance. Supports arbitrary labels. See the\n"
"README for more information.\n"
"\n"
"Copyright 2009-2010 Chris Whidden\n"
"whidden@cs.dal.ca\n"
"http://kiwi.cs.dal.ca/Software/RSPR\n"
"March 22, 2010\n"
"Version 1.01\n"
"\n"
"This program comes with ABSOLUTELY NO WARRANTY.\n"
"This is free software, and you are welcome to redistribute it\n"
"under certain conditions; See the README for details.\n"
"\n"
"*******************************************************************************\n"
"ALGORITHM\n"
"*******************************************************************************\n"
"\n"
"These options control what algorithm is used\n"
"\n"
"-fpt        Calculate the exact rSPR distance with an FPT algorithm\n"
"\n"
"-bb         Calculate the exact rSPR distance with a branch-and-bound\n"
"            FPT algorithm. This is the default option.\n"
"\n"
"-approx     Calculate just a linear -time 3-approximation of the\n"
"            rSPR distance\n"
"\n"
"*******************************************************************************\n"
"OPTIMIZATIONS\n"
"*******************************************************************************\n"
"\n"
"These options control the use of optimized branching. All optimizations are\n"
"enabled by default. Specifying any subset of -cob, -cab, and -sc will use\n"
"just that subset of optimizations. See the README for more information.\n"
"\n"
"-allopt     Use -cob -cab -sc. This is the default option\n"
"\n"
"-noopt      Use 3-way branching for all FPT algorithms\n"
"\n"
"-cob        Use \"cut one b\" improved branching\n"
"\n"
"-cab        Use \"cut all b\" improved branching\n"
"\n"
"-sc         Use \"separate components\" improved branching\n"
"\n"
"*******************************************************************************\n"
"UNROOTED COMPARISON OPTIONS\n"
"*******************************************************************************\n"
"\n"
"-unrooted   Compare the first input tree to each other input tree.\n"
"            Output the best found distance and agreement forest\n"
"-unrooted_min_approx\n"
"            Compare the first input tree to each other input tree.\n"
"            Run the exact algorithms on the pair with the\n"
"            minimum approximate rspr distance\n"
"\n"
"*******************************************************************************\n"
"OTHER OPTIONS\n"
"*******************************************************************************\n"
"-cc         Calculate a potentially better approximation with a quadratic time\n"
"            algorithm\n"
"\n"
"-q          Quiet; Do not output the input trees or approximation\n"
"*******************************************************************************\n";

int main(int argc, char *argv[]) {
	int max_args = argc-1;
	while (argc > 1) {
		char *arg = argv[--argc];
		if (strcmp(arg, "-fpt") == 0) {
			FPT = true;
			DEFAULT_ALGORITHM=false;
		}
		else if (strcmp(arg, "-bb") == 0) {
			BB = true;
			DEFAULT_ALGORITHM=false;
		}
		else if (strcmp(arg, "-approx") == 0) {
			DEFAULT_ALGORITHM=false;
		}
		else if (strcmp(arg, "-q") == 0)
			QUIET = true;
		else if (strcmp(arg, "-cc") == 0)
			APPROX_CHECK_COMPONENT = true;
		else if (strcmp(arg, "-unrooted") == 0)
			UNROOTED = true;
		else if (strcmp(arg, "-unrooted_min_approx") == 0)
			UNROOTED_MIN_APPROX = true;
		else if (strcmp(arg, "-noopt") == 0) {
			DEFAULT_OPTIMIZATIONS=false;
		}
		else if (strcmp(arg, "-cut_one_b") == 0 ||
				strcmp(arg, "-cob") == 0) {
			CUT_ONE_B = true;
			DEFAULT_OPTIMIZATIONS=false;
		}
		else if (strcmp(arg, "-cut_all_b") == 0 ||
				strcmp(arg, "-cab") == 0) {
			CUT_ALL_B = true;
			DEFAULT_OPTIMIZATIONS=false;
		}
		else if (strcmp(arg, "-cut_ac_separate_components") == 0 ||
				strcmp(arg, "-sc") == 0) {
			CUT_AC_SEPARATE_COMPONENTS = true;
			DEFAULT_OPTIMIZATIONS=false;
		}
		else if (strcmp(arg, "-cut_one_ab") == 0) {
			CUT_ONE_AB = true;
			DEFAULT_OPTIMIZATIONS=false;
		}
		else if (strcmp(arg, "-h") == 0) {
			cout << USAGE;
			return 0;
		}
		else if (strcmp(arg, "-lca") == 0) {
			LCA_TEST = true;
		}
		else if (strcmp(arg, "-cluster") == 0) {
			CLUSTER_REDUCTION = true;
			PREFER_RHO = true;
			if (max_args > argc) {
				char *arg2 = argv[argc+1];
				if (arg2[0] != '-')
					MAX_CLUSTERS = atoi(arg2);
				cout << "MAX_CLUSTERS=" << MAX_CLUSTERS << endl;
			}
		}
		else if (strcmp(arg, "-cluster_test") == 0) {
			CLUSTER_TEST = true;
			PREFER_RHO = true;
		}
		else if (strcmp(arg, "-prefer_rho") == 0) {
			PREFER_RHO = true;
		}
		else if (strcmp(arg, "-memoize") == 0) {
			MEMOIZE = true;
		}
		else if (strcmp(arg, "-all_mafs") == 0) {
			ALL_MAFS= true;
		}
		else if (strcmp(arg, "--help") == 0) {
			cout << USAGE;
			return 0;
		}
			
	}
	if (DEFAULT_OPTIMIZATIONS) {
		CUT_ALL_B=true;
		CUT_ONE_B = true;
		CUT_AC_SEPARATE_COMPONENTS = true;
	}
	if (DEFAULT_ALGORITHM) {
		BB=true;
	}

	// Label maps to allow string labels
	map<string, int> label_map= map<string, int>();
	map<int, string> reverse_label_map = map<int, string>();

	// Normal operation
	if (!UNROOTED && !UNROOTED_MIN_APPROX) {
		string T1_line = "";
		string T2_line = "";
		while (getline(cin, T1_line) && getline(cin, T2_line)) {
			Node *T1 = build_tree(T1_line);
			Node *T2 = build_tree(T2_line);


			vector<Node *> leaves = T1->find_leaves();
			int n = leaves.size();
			cout << "T1: ";
			T1->print_subtree();
			T1->preorder_number();
			cout << endl;
			for(int i = 0; i <  2*n - 3  + 1; i++) {
				T1->next_rooting();
				cout << "T1: ";
				cout << T1->str_subtree();
	//			cout << endl;
				cout << "\t" << T1->lchild()->get_preorder_number() << "  " << T1->rchild()->get_preorder_number();
				if ( T1->lchild()->get_preorder_number() > T1->rchild()->get_preorder_number())
					cout << " * ";
				cout << endl;
			}

			cout << endl;



			// TODO: should we sync here to prune out additional leaves?
			if (!QUIET) {
				cout << "T1: ";
				T1->print_subtree();
				cout << "T2: ";
				T2->print_subtree();
				cout << endl;
			}
			if (LCA_TEST) {
				LCA lca_query = LCA(T1);
				cout << endl;
				lca_query.debug();
				cout << endl;
				vector<Node *> leaves = T1->find_leaves();
				for(vector<Node *>::iterator i = leaves.begin(); i != leaves.end(); i++) {
					for(vector<Node *>::iterator j = i; j != leaves.end(); j++) {
						if (j==i)
							continue;
						Node *lca = lca_query.get_lca(*i, *j);
						(*i)->print_subtree_hlpr();
						cout << "\t";
						(*j)->print_subtree_hlpr();
						cout << "\t";
						lca->print_subtree();
					}
				}
				return 0;
			}

			T1->labels_to_numbers(&label_map, &reverse_label_map);
			T2->labels_to_numbers(&label_map, &reverse_label_map);
	
//			ClusterForest F1 = ClusterForest(T1);
//			ClusterForest F2 = ClusterForest(T2);
//			ClusterForest F3 = ClusterForest(T1);
//			ClusterForest F4 = ClusterForest(T2);
			ClusterForest F1 = ClusterForest(T1);
			ClusterForest F2 = ClusterForest(T2);
			Forest F3 = Forest(T1);
			Forest F4 = Forest(T2);

			if (CLUSTER_TEST) {
				int approx_spr = rSPR_3_approx(&F3, &F4);
					cout << "approx drSPR=" << approx_spr << endl;
					cout << "\n";

				sync_twins(&F1, &F2);
				//sync_interior_twins_real(&F1, &F2);
				sync_interior_twins(&F1, &F2);
				list<Node *> *cluster_points = find_cluster_points(&F1);
//				if (!cluster_points->empty()) {
					/*
					cout << "CLUSTER POINTS:" << endl;
					for(list<Node *>::iterator i = cluster_points->begin();
							i != cluster_points->end(); i++) {
						string s1 = (*i)->str_subtree();
						string s2 = (*i)->get_twin()->str_subtree();
						if (s1 != s2)
							cout << s1 << "\t" << s2 << endl;
					}
					cout << endl;
					*/
					// TODO: not quite right because we need to put
					// in the leaf with a ref and not do contractions
					for(list<Node *>::iterator i = cluster_points->begin();
							i != cluster_points->end(); i++) {
						stringstream ss;
						string cluster_name = "X";
						ss << F1.size();
						cluster_name += ss.str();
						int num_labels = label_map.size();
						label_map.insert(make_pair(cluster_name,num_labels));
						reverse_label_map.insert(
								make_pair(num_labels,cluster_name));
						ss.str("");
						ss << num_labels;
						cluster_name = ss.str();

						Node *n = *i;
						Node *n_parent = n->parent();
						Node *twin = n->get_twin();
						Node *twin_parent = twin->parent();

						F1.add_cluster(n,cluster_name);

						F2.add_cluster(twin,cluster_name);

						Node *n_cluster =
								F1.get_cluster_node(F1.num_clusters()-1);
						Node *twin_cluster =
								F2.get_cluster_node(F2.num_clusters()-1);
						n_cluster->set_twin(twin_cluster);
						twin_cluster->set_twin(n_cluster);
					}
					cout << endl << "CLUSTERS" << endl;
					//cout << "F1: " << endl;
					//F1.print_components("\n");
					//cout << endl;
					//cout << "F2: " << endl;
					//F2.print_components("\n");

					// component 0 needs to be done last
					F1.add_component(F1.get_component(0));
					F2.add_component(F2.get_component(0));

					int exact_spr;
					int k;
					int num_clusters = F1.num_components();
					int total_k = 0;
					for(int i = 1; i < num_clusters; i++) {
						Forest f1 = Forest(F1.get_component(i));

						Forest f2 = Forest(F2.get_component(i));
						Forest f1a = Forest(f1);
						Forest f2a = Forest(f2);
						Forest *f1_cluster;
						Forest *f2_cluster;

//						f1.numbers_to_labels(&reverse_label_map);
//						f2.numbers_to_labels(&reverse_label_map);
						cout << "C" << i << "_1: ";
						f1.print_components();
						cout << "C" << i << "_2: ";
						f2.print_components();

						int approx_spr = rSPR_worse_3_approx(&f1a, &f2a);
						if (!(QUIET && (BB || FPT)))
							cout << "cluster approx drSPR=" << approx_spr << endl;

						cout << endl;

						if (FPT || BB) {
							int min_spr = approx_spr / 3;
							for(k = min_spr; k <= MAX_SPR; k++) {
								Forest f1t = Forest(f1);
								Forest f2t = Forest(f2);
								f1t.unsync();
								f2t.unsync();
								cout << k << " ";
								cout.flush();
//								cout << "foo1" << endl;
//								f1t.print_components();
//								f1t.print_components();
//								cout << "foo1a" << endl;
								exact_spr = rSPR_branch_and_bound(&f1t, &f2t, k);
//								cout << "foo2" << endl;
								if (exact_spr >= 0) {
//									cout << "foo3" << endl;
//										cout << "foo3a" << endl;
//										cout << "rho=" << f1t.contains_rho() << endl;
//										cout << i << endl;
//										cout << &f1t << endl;
//										cout << f1t.num_components() << endl;
//										f1t.print_components();
										// TODO: problem things getting
										// deleted?
									if ( i < num_clusters - 1) {
										F1.join_cluster(i,&f1t);
//										cout << "foo3b" << endl;
//										cout << "rho=" << f2t.contains_rho() << endl;
//										F2.print_components();
//										f2t.print_components();
										F2.join_cluster(i,&f2t);
//									cout << "foo4" << endl;
									}
									else {
										F1.join_cluster(&f1t);
										F2.join_cluster(&f2t);
									}
									//f1.numbers_to_labels(&reverse_label_map);
									//f2.numbers_to_labels(&reverse_label_map);
									cout << endl;
									cout << "F" << i << "_1: ";
									f1t.print_components();
//									cout << "foo5" << endl;
									cout << "F" << i << "_2: ";
									f2t.print_components();
//									cout << "foo6" << endl;
									cout << "cluster exact drSPR=" << exact_spr << endl;
									cout << endl;
									total_k += exact_spr;
									break;
								}
							}
							if (exact_spr == -1)
							cout << "exact drSPR=?  " << "k=" << k << " too large" << endl;
							cout << "\n";
						}
					}
				//}

				if (BB | FPT) {
					if (F1.contains_rho()) {
						F1.erase_components(0, num_clusters);
						F2.erase_components(0, num_clusters);
					}
					else {
						F1.erase_components(1, num_clusters+1);
						F2.erase_components(1, num_clusters+1);
					}
					cout << "F1: ";
					F1.print_components();
					cout << "F2: ";
					F2.print_components();
					cout << "total exact drSPR=" << total_k << endl;
				}

				T1->delete_tree();
				T2->delete_tree();
				delete cluster_points;
				return 0;
			}
	
			// APPROX ALGORITHM
			int approx_spr = rSPR_worse_3_approx(&F1, &F2);
			int min_spr = approx_spr / 3;
			if (!(QUIET && (BB || FPT))) {
				F1.numbers_to_labels(&reverse_label_map);
				F2.numbers_to_labels(&reverse_label_map);
				cout << "F1: ";
				F1.print_components();
				cout << "F2: ";
				F2.print_components();
				cout << "approx drSPR=" << approx_spr << endl;
				cout << "\n";
			}
	
			int k = min_spr;

			/*
			// FPT ALGORITHM
			int exact_spr = -1;
			int k = min_spr;
			if (FPT) {
				for(k = min_spr; k <= MAX_SPR; k++) {
					cout << k << " ";
					cout.flush();
					Forest F1 = Forest(F3);
					Forest F2 = Forest(F4);
					exact_spr = rSPR_FPT(&F1, &F2, k);
					if (exact_spr >= 0) {
						F1.numbers_to_labels(&reverse_label_map);
						F2.numbers_to_labels(&reverse_label_map);
						cout << endl;
						cout << "F1: ";
						F1.print_components();
						cout << "F2: ";
						F2.print_components();
						cout << "exact drSPR=" << exact_spr << endl;
						T1->delete_tree();
						T2->delete_tree();
						continue;
					}
				}
				if (exact_spr == -1)
					cout << "exact drSPR=?  " << "k=" << k << " too large" << endl;
				cout << "\n";
			}
			*/
		
			if (BB || FPT) {
				// BRANCH AND BOUND FPT ALGORITHM
				Forest F1 = Forest(F3);
				Forest F2 = Forest(F4);
				int exact_spr = rSPR_branch_and_bound(&F1, &F2);
				if (exact_spr >= 0) {
					cout << "F1: ";
					F1.print_components();
					cout << "F2: ";
					F2.print_components();
					F1.numbers_to_labels(&reverse_label_map);
					F2.numbers_to_labels(&reverse_label_map);
					cout << endl;
					cout << "F1: ";
					F1.print_components();
					cout << "F2: ";
					F2.print_components();
					cout << "exact BB drSPR=" << exact_spr << endl;
					T1->delete_tree();
					T2->delete_tree();
					continue;
				}
				if (exact_spr == -1)
						cout << "exact BB drSPR=?  " << "k=" << k << " too large"
							<< endl;
				cout << "\n";
			}
	
			// cleanup
			T1->delete_tree();
			T2->delete_tree();
		}
	}
	// Comparison between a rooted tree and all rootings of an unrooted tree
	else if (UNROOTED || UNROOTED_MIN_APPROX) {
		string line = "";
		vector<Forest> trees = vector<Forest>();
		if (!getline(cin, line))
			return 0;
		Node *T1 = build_tree(line);
		if (!QUIET) {
			cout << "T1: ";
			T1->print_subtree();
		}
		T1->labels_to_numbers(&label_map, &reverse_label_map);
		Forest F1 = Forest(T1);
		while (getline(cin, line)) {
			Node *T2 = build_tree(line);
			if (!QUIET) {
				cout << "T2: ";
				T2->print_subtree();
			}
			T2->labels_to_numbers(&label_map, &reverse_label_map);
			trees.push_back(Forest(T2));
		}
		cout << endl;

		if (trees.size() == 0)
			return 0;


		// APPROX ALGORITHM
		int min_spr = (int)1E9;
		int min_i = 0;
		vector<int> approx_spr = vector<int>(trees.size());
		for (int i = 0; i < trees.size(); i++) {
			Forest F3 = Forest(F1);
			Forest F4 = Forest(trees[i]);
			approx_spr[i] = rSPR_worse_3_approx(&F3, &F4);
			if (approx_spr[i] < min_spr) {
				min_spr = approx_spr[i];
				min_i = i;
			}
			if (!(QUIET && (BB || FPT)) && !UNROOTED_MIN_APPROX) {
				F3.numbers_to_labels(&reverse_label_map);
				F4.numbers_to_labels(&reverse_label_map);
				cout << "F1: ";
				F3.print_components();
				cout << "F2: ";
				F4.print_components();
				cout << "approx drSPR=" << approx_spr[i] << endl;
				cout << "\n";
			}
		}
		// Choose a rooting with minimum approximate distance
		if (UNROOTED_MIN_APPROX) {
			Forest min_approx_forest = trees[min_i];
			trees.clear();
			trees.push_back(min_approx_forest);
				F1.numbers_to_labels(&reverse_label_map);
				min_approx_forest.numbers_to_labels(&reverse_label_map);
				cout << "F1: ";
				F1.print_components();
				cout << "F2: ";
				min_approx_forest.print_components();
				F1.labels_to_numbers(&label_map, &reverse_label_map);
				min_approx_forest.labels_to_numbers(&label_map, &reverse_label_map);
		}

		cout << "min approx drSPR=" << min_spr << endl;
		cout << "\n";

		min_spr /= 3;

		int k, exact_spr;
		if (FPT || BB) {
			// BRANCH AND BOUND FPT ALGORITHM
			for(k = min_spr; k <=MAX_SPR;  k++) {
				cout << k << " ";
				cout.flush();
				for (int i = 0; i < trees.size(); i++) {
					Forest F3 = Forest(F1);
					Forest F4 = Forest(trees[i]);
					exact_spr = rSPR_branch_and_bound(&F3, &F4, k);
					if (exact_spr >= 0) {
						sync_twins(&F1, &trees[i]);
						F1.numbers_to_labels(&reverse_label_map);
						trees[i].numbers_to_labels(&reverse_label_map);
						F3.numbers_to_labels(&reverse_label_map);
						F4.numbers_to_labels(&reverse_label_map);
						cout << endl;
						cout << "T1: ";
						F1.print_components();
						cout << "T2: ";
						trees[i].print_components();
						cout << endl;
						cout << "F1: ";
						F3.print_components();
						cout << "F2: ";
						F4.print_components();
						cout << "exact BB drSPR=" << exact_spr << endl;
						continue;
					}
				}
				if (exact_spr >= 0) {
					continue;
				}
			}
			if (exact_spr == -1)
				cout << "exact BB drSPR=?  " << "k=" << k << " too large" << endl;
			cout << "\n";
		}

	}
	return 0;
}

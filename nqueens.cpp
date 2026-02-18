#include <iostream>
#include <vector>
#include <set>
#include <algorithm>
#ifdef CILK
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
#include <cilk/reducer_opadd.h>
#include <cilk/reducer_vector.h>
#else
#define cilk_spawn
#define cilk_sync
#endif
using namespace std;

cilk::reducer_opadd<int> total_count(0);

vector<int> rotate90(const vector<int> &v)
{
    int N = v.size();
    vector<int> res(N);
    for (int i = 0; i < N; i++)
        res[v[i]] = N - 1 - i;
    return res;
}

vector<int> reflect(const vector<int> &v)
{
    int N = v.size();
    vector<int> res(N);
    for (int i = 0; i < N; i++)
        res[i] = N - 1 - v[i];
    return res;
}

vector<int> canonical(const vector<int> &sol)
{
    vector<vector<int>> forms;
    vector<int> temp = sol;

    for (int i = 0; i < 4; i++)
    {
        forms.push_back(temp);
        forms.push_back(reflect(temp));
        temp = rotate90(temp);
    }

    return *min_element(forms.begin(), forms.end());
}

void check3(int row,int nq,vector<vector<int>> &board,vector<bool> &col,vector<bool> &diag1,vector<bool> &diag2,cilk::reducer<cilk::op_vector<vector<int>>> &solution,vector<int> &temp,int cutoff,int depth,int stopafter)
{
    if (stopafter != -1 && total_count.get_value() >= stopafter)
        return;
    int N = board.size();

    // Base case
    if (row == N)
    {
        solution->push_back(temp);
        total_count += 1;
        return;
    }

    for (int j = 0; j < N; j++)
    {   
        if (stopafter != -1 && total_count.get_value() >= stopafter)
            break;


        if (!col[j] && !diag1[row + j] && !diag2[row - j + N])
        {

            // If parallel region
            if (cutoff == -1 || depth < cutoff)
            {

                // Create copies for parallel safety
                auto board_copy = board;
                auto col_copy = col;
                auto diag1_copy = diag1;
                auto diag2_copy = diag2;
                auto temp_copy = temp;

                board_copy[row][j] = 1;
                col_copy[j] = true;
                diag1_copy[row + j] = true;
                diag2_copy[row - j + N] = true;
                temp_copy.push_back(j);

                cilk_spawn check3(row + 1,nq - 1,board_copy,col_copy,diag1_copy,diag2_copy,solution,temp_copy,cutoff,depth + 1,stopafter);
            }
            else
            {

                // Serial branch (safe to modify in place)
                board[row][j] = 1;
                col[j] = true;
                diag1[row + j] = true;
                diag2[row - j + N] = true;
                temp.push_back(j);

                check3(row + 1,nq - 1,board,col,diag1,diag2,solution,temp,cutoff,depth + 1,stopafter);

                // Backtrack
                board[row][j] = 0;
                col[j] = false;
                diag1[row + j] = false;
                diag2[row - j + N] = false;
                temp.pop_back();
            }
        }
    }

    cilk_sync;
}

void check2(int nq,vector<vector<int>> &board,cilk::reducer<cilk::op_vector<vector<int>>> &solution,int cutoff,int stopafter)
{
    int N = board.size();

    vector<bool> col(N, false);
    vector<bool> diag1(2 * N, false);
    vector<bool> diag2(2 * N, false);

    vector<int> temp;

    check3(0,nq,board,col,diag1,diag2,solution,temp,cutoff,0,stopafter);
}

int main(int argc, char **argv)
{

    if (argc != 4)
    {
        cout << "Usage: ./nqueens <N> <cutoff> <stopafter>\n";
        return 0;
    }

    int n = atoi(argv[1]);
    int cutoff = atoi(argv[2]);
    int stopafter = atoi(argv[3]);

    vector<vector<int>> board(n, vector<int>(n, 0));
    cilk::reducer<cilk::op_vector<vector<int>>> solution;

    total_count.set_value(0);
    check2(n, board, solution, cutoff, stopafter);

    vector<vector<int>> all_solutions = solution.get_value();

    cout << "All solutions:\n";
    for (int i = 0; i < all_solutions.size(); i++)
    {
        cout << "solution " << i << ": ";
        for (int j : all_solutions[i])
            cout << j << " ";
        cout << "\n";
    }

    set<vector<int>> unique_set;

    for (auto &sol : all_solutions)
        unique_set.insert(canonical(sol));

    cout << "Unique solutions:\n";
    int idx = 0;
    for (auto &sol : unique_set)
    {
        cout << "unique solution " << idx++ << ": ";
        for (int j : sol)
            cout << j << " ";
        cout << "\n";
    }

    cout << "Number of solutions: " << all_solutions.size() << "\n";
    cout << "Number of unique solutions: " << unique_set.size() << "\n";

    return 0;
}

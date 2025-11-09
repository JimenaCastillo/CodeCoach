long long multiplyLoop(vector<int>& nums) {
    long long total = 1;
    for (int x : nums) {
        total *= (x + 1);
    }
    return total;
}

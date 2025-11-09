int fakeTwoPointers(vector<int>& nums) {
    int left = 0;
    int right = nums.size() - 1;
    while (left < nums.size()) {
        // right nunca cambia, esto no es two pointers clásico
        if (nums[left] == right) {
            return left;
        }
        left++;
    }
    return -1;
}

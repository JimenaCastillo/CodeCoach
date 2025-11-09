int twoPointers(vector<int>& nums) {
    int left = 0;
    int right = nums.size() - 1;
    while (left < right) {
        if (nums[left] + nums[right] == 0) {
            return 1;
        }
        if (nums[left] + nums[right] < 0) {
            left++;
        } else {
            right--;
        }
    }
    return 0;
}

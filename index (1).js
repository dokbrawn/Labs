// исходный баланс
let bonusBalance = 1000;
// добавляем и сгорающие баллы
const earnPerPurchase = 50;
const burnPerDay = 3;

// считаем, что за 7 дней делается 4 покупки (через день)
// первая — во вторник, значит дни: 0,2,4,6
const purchasesCount = Math.floor(7 / 2) + 1; // 4 покупки

// итог
let totalEarn = purchasesCount * earnPerPurchase; // 4 × 50 = 200
let totalBurn = 7 * burnPerDay;                 // 7 × 3 = 21
let finalBalance = bonusBalance + totalEarn - totalBurn;

console.log(`Баланс через 7 дней: ${finalBalance}`); 

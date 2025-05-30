const messages = [
  "Пойдем гулять в парк?",
  "Кажется, дождь собирается. Лучше пойдем в кино!",
  "Давай, сегодня как раз вышел новый фильм.",
  "Встречаемся через час у кинотеатра."
];

const searchTerm = "кино";

// Поиск и вывод сообщений, содержащих искомый текст
messages.forEach((message, index) => {
  if (message.includes(searchTerm)) {
    const sender = index % 2 === 0 ? "Друг" : "Вы";
    console.log(`${sender}: ${message}`);
  }
});
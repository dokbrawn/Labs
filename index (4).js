// возвращает Promise с массивом объектов
export async function fetchCharacters() {
  const response = await fetch('https://jsfree-les-3-api.onrender.com/characters');
  if (!response.ok) {
    throw new Error(`Ошибка HTTP: ${response.status}`);
  }
  return response.json();
}
// на вход — массив characters, на выход — массив HTML-строк
export function getCharacterCards(characters) {
  return characters.map(ch => getCharacterCard(ch));
}

// вспомогательная функция для одной карточки
function getCharacterCard(ch) {
  return `
    <div class="col-md-4 mb-4">
      <div class="card h-100">
        <img src="${ch.image}" class="card-img-top" alt="${ch.name}">
        <div class="card-body">
          <h5 class="card-title">${ch.name}</h5>
          <p class="card-text">${ch.origin}</p>
          <button 
            class="btn btn-primary" 
            data-bs-toggle="modal" 
            data-bs-target="#modal-${ch.id}">
            Подробнее
          </button>
        </div>
      </div>
    </div>`;
}
// на вход — массив characters, на выход — массив HTML-строк модалок
export function getCharacterModals(characters) {
  return characters.map(ch => getCharacterModal(ch));
}

// вспомогательная функция для одной модалки
function getCharacterModal(ch) {
  return `
    <div class="modal fade" id="modal-${ch.id}" tabindex="-1" aria-labelledby="label-${ch.id}" aria-hidden="true">
      <div class="modal-dialog modal-lg modal-dialog-centered">
        <div class="modal-content">
          <div class="modal-header">
            <h5 class="modal-title" id="label-${ch.id}">${ch.name}</h5>
            <button type="button" class="btn-close" data-bs-dismiss="modal" aria-label="Закрыть"></button>
          </div>
          <div class="modal-body d-flex">
            <img src="${ch.image}" class="img-fluid me-3" alt="${ch.name}" style="max-width:200px;">
            <div>
              <p><strong>Происхождение:</strong> ${ch.origin}</p>
              <p><strong>Описание:</strong> ${ch.description || 'Нет данных'}</p>
            </div>
          </div>
          <div class="modal-footer">
            <button type="button" class="btn btn-secondary" data-bs-dismiss="modal">Закрыть</button>
          </div>
        </div>
      </div>
    </div>`;
}
